#include "pv_policy.h"
#include "pv_moonraker.h"
#include "pv_motor.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "pv_policy";

#define NVS_NS       "app_nvs"
#define KEY_MODE     "policy_mode"

// Bed-temperature hysteresis. Open the vent above BED_OPEN_C, close below
// BED_CLOSE_C, hold current state between. Values chosen conservatively so
// residual heat after a print keeps the vent open until the chamber cools.
#define BED_OPEN_C   45.0f
#define BED_CLOSE_C  35.0f
#define TICK_MS      1000

// Material-aware behavior. When the printer publishes a known material name,
// PLA/PETG/etc. prefer venting for cooling; ABS/ASA/PC prefer sealing for
// heat retention. If the material is unknown, we fall back to the temperature
// hysteresis above.
//
// Bambu's stock firmware brags about this on the product page but doesn't
// document the exact rules — these defaults are conservative and match what
// most enclosure users do by hand.
typedef enum {
    MAT_PREFER_UNKNOWN,   // no rule; use temperature hysteresis
    MAT_PREFER_OPEN,      // cool-running plastics (PLA, PETG, TPU)
    MAT_PREFER_SEALED,    // hot-chamber plastics (ABS, ASA, PC, PA)
} material_pref_t;

static material_pref_t material_preference(const char *m)
{
    if (m == NULL || m[0] == '\0') return MAT_PREFER_UNKNOWN;
    // Match on prefix so "PLA_PLUS", "PETG-CF", "ABS+", etc. classify correctly.
    if (strncmp(m, "PLA",  3) == 0) return MAT_PREFER_OPEN;
    if (strncmp(m, "PETG", 4) == 0) return MAT_PREFER_OPEN;
    if (strncmp(m, "PET",  3) == 0) return MAT_PREFER_OPEN;
    if (strncmp(m, "TPU",  3) == 0) return MAT_PREFER_OPEN;
    if (strncmp(m, "ABS",  3) == 0) return MAT_PREFER_SEALED;
    if (strncmp(m, "ASA",  3) == 0) return MAT_PREFER_SEALED;
    if (strncmp(m, "PC",   2) == 0) return MAT_PREFER_SEALED;
    if (strncmp(m, "PA",   2) == 0) return MAT_PREFER_SEALED;   // nylon
    if (strncmp(m, "HIPS", 4) == 0) return MAT_PREFER_SEALED;
    return MAT_PREFER_UNKNOWN;
}

static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t      s_task = NULL;
static pv_policy_mode_t  s_mode = PV_POLICY_MODE_AUTO;
static pv_motor_target_t s_manual_target  = PV_MOTOR_TARGET_CLOSED;
static pv_motor_target_t s_current_target = PV_MOTOR_TARGET_CLOSED;

static void apply_target(pv_motor_target_t t)
{
    int n = pv_motor_active_groups();
    for (int g = 0; g < n; ++g) pv_motor_set_target(g, t);
    s_current_target = t;
}

// AUTO decision. Returns the target we should be driving toward, given the
// current Moonraker snapshot. If we don't have reliable data, keep whatever
// we're already commanding.
//
// Order of consideration:
//   1. No subscription yet / unknown state -> hold
//   2. ERROR                               -> hold (don't move on a broken printer)
//   3. Printing/preparing/paused + material rule wants sealed -> CLOSED
//   4. Printing/preparing/paused           -> OPEN
//   5. Idle/complete, bed still hot        -> OPEN  (residual heat)
//   6. Idle/complete, bed cool             -> CLOSED
//   7. Otherwise                           -> hold  (hysteresis band)
static pv_motor_target_t decide_auto_target(const pv_moonraker_status_t *st)
{
    if (st->state != PV_MK_SUBSCRIBED)     return s_current_target;
    if (st->printer == PV_PRINTER_UNKNOWN) return s_current_target;
    if (st->printer == PV_PRINTER_ERROR)   return s_current_target;

    bool active = (st->printer == PV_PRINTER_PRINTING  ||
                   st->printer == PV_PRINTER_PREPARING ||
                   st->printer == PV_PRINTER_PAUSED);

    if (active) {
        material_pref_t mat = material_preference(st->material);
        if (mat == MAT_PREFER_SEALED) return PV_MOTOR_TARGET_CLOSED;
        // MAT_PREFER_OPEN and MAT_PREFER_UNKNOWN both open during a print —
        // the tester's ask is that PLA-style vents while ABS seals; when we
        // don't know the material we default to venting, which is the safer
        // choice for PLA-family plastics that dominate hobby printing.
        return PV_MOTOR_TARGET_OPEN;
    }

    // Idle / complete: use bed-temp hysteresis so residual chamber heat
    // keeps the vent open until things cool down.
    if (st->bed_temp > BED_OPEN_C)  return PV_MOTOR_TARGET_OPEN;
    if (st->bed_temp < BED_CLOSE_C) return PV_MOTOR_TARGET_CLOSED;
    return s_current_target;
}

static void policy_task(void *arg)
{
    (void)arg;
    for (;;) {
        pv_moonraker_status_t st;
        pv_moonraker_get_status(&st);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        pv_motor_target_t want = (s_mode == PV_POLICY_MODE_MANUAL)
                                     ? s_manual_target
                                     : decide_auto_target(&st);
        if (want != s_current_target) {
            ESP_LOGI(TAG, "target -> %s (mode=%s, printer=%s, mat=%s, bed=%.1f)",
                     want == PV_MOTOR_TARGET_OPEN   ? "OPEN"
                   : want == PV_MOTOR_TARGET_CLOSED ? "CLOSED" : "STOP",
                     s_mode == PV_POLICY_MODE_AUTO ? "AUTO" : "MANUAL",
                     pv_printer_state_str(st.printer),
                     st.material[0] ? st.material : "?",
                     st.bed_temp);
            apply_target(want);
        }
        xSemaphoreGive(s_lock);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

// ---------- NVS ----------

static void load_mode(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t m = 0;
    if (nvs_get_u8(h, KEY_MODE, &m) == ESP_OK) {
        s_mode = (m == PV_POLICY_MODE_MANUAL) ? PV_POLICY_MODE_MANUAL
                                              : PV_POLICY_MODE_AUTO;
    }
    nvs_close(h);
}

static void save_mode(pv_policy_mode_t m)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, KEY_MODE, (uint8_t)m);
    nvs_commit(h);
    nvs_close(h);
}

// ---------- public API ----------

esp_err_t pv_policy_start(void)
{
    if (s_task != NULL) return ESP_ERR_INVALID_STATE;
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) return ESP_ERR_NO_MEM;
    load_mode();
    if (xTaskCreate(policy_task, "pv_policy", 4096, NULL, 4, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started in %s mode", s_mode == PV_POLICY_MODE_AUTO ? "AUTO" : "MANUAL");
    return ESP_OK;
}

esp_err_t pv_policy_set_mode(pv_policy_mode_t mode)
{
    if (s_lock == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_mode = mode;
    save_mode(mode);
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

pv_policy_mode_t pv_policy_get_mode(void) { return s_mode; }

esp_err_t pv_policy_set_manual_target(pv_motor_target_t t)
{
    if (s_lock == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_manual_target = t;
    // If we're already in manual mode, apply immediately instead of waiting
    // for the next tick.
    if (s_mode == PV_POLICY_MODE_MANUAL && t != s_current_target) apply_target(t);
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

pv_motor_target_t pv_policy_get_target(void) { return s_current_target; }

esp_err_t pv_policy_clear(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_key(h, KEY_MODE);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}
