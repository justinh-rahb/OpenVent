# OpenVent Roadmap

Custom firmware for the Bigtreetech Panda Vent hardware that replaces the
stock Bambu Lab MQTT integration with Moonraker/Klipper support.

## Goals

- Repurpose stock Panda Vent hardware for Klipper-based printers
- Automatically open/close the vent based on printer status, material, and
  chamber conditions — matching (and eventually exceeding) stock behavior
- Provide a captive portal AP for WiFi setup (like stock firmware)
- Simple web UI for Moonraker connection, thresholds, and material rules
- OTA firmware updates via web UI
- Complete feature parity with the stock Bigtreetech firmware where it
  makes sense; skip cloud-only features

## Phase 1 — MVP (Hardware Bring-up & Core Logic) — ✅ shipped in v0.2.6

Baseline "vents move on real hardware, don't crash, portal works" release.
Verified against tester OldGuyMeltsPlastic's retail 2-vent kit on
2026-07-10 — 10× open/close cycles with no ESP crash.

### Vent Control via Moonraker
- [x] Connect to Moonraker via WebSocket (`pv_moonraker`)
- [x] Subscribe to `print_stats` and `heater_bed` objects
- [x] Open vent when bed is heated / printing is active (`pv_policy`, AUTO mode)
- [x] Close vent when bed cools down / printer is idle (35 °C / 45 °C hysteresis)
- [x] Auto/manual mode toggle via physical button (GPIO 12)
- [x] Long-press button to switch between auto and manual mode

### WiFi & Captive Portal
- [x] AP mode on first boot with DNS-redirect captive portal
- [x] Web page to enter SSID + password
- [x] Credentials stored in NVS
- [x] Auto-reconnect on boot
- [x] mDNS `OpenVent.local`

### Motor + Hall
- [x] LEDC PWM forward/reverse with fade-based soft-start
- [x] Hall-sensor position feedback with arrival debounce (30 ms) and
      finite retry (`gave_up` flag) — v0.2.6
- [x] Widened CLOSED band to survive the non-monotonic mid-travel hump

### Portal Parity (Web UI)
- [x] Tabbed layout (Home / WiFi / Printer / System)
- [x] Persistent status card (fw version, WiFi, Moonraker, printer state, bed, target, mode)
- [x] Quick Open / Close buttons
- [x] Dark mode via `prefers-color-scheme`
- [x] WiFi scan + hidden-SSID entry, configurable AP hotspot, AP toggle
- [x] OTA `.bin` upload → inactive partition
- [x] Factory reset

### Known limitations we're carrying into 0.3.x

- **Hall thresholds** are hard-coded (raw ADC counts). Work for OldGuy's
  board — unknown whether every board will match. Stock does a per-boot
  ADC calibration; we don't. See [Phase 3](#phase-3--hall-sensor-calibration-parity).
- **CLOSED** currently arrives on the mid-travel bump (~2260 raw) rather
  than the true settled endpoint (~1374 raw). Mechanically OK because the
  physical hard-stop lands at the same time, but semantically off. Tighten
  the band once we understand the sensor curve better.
- **RGB status LEDs** are not driven yet. Deferred beyond 0.3.x.

## Phase 2 — Deeper Moonraker Integration & Auto Logic (v0.3.0)

Right now the "auto" policy is `printing OR bed>30 → OPEN`. That's enough
to prove it works but ignores everything else Klipper knows. Goal for
0.3.0: read Moonraker as richly as stock reads Bambu MQTT, and use that
data to make smarter open/close decisions — including material-aware
behavior (the tester's core ask).

### Moonraker ingest expansion (`pv_moonraker`)
- [ ] Expand the initial subscribe: add `virtual_sdcard`, `webhooks`,
      `display_status`, `extruder`, and optional `heater_generic chamber`
      alongside the existing `print_stats` + `heater_bed`
- [ ] Expose a `pv_printer_state_t` enum instead of a bare `printing`
      bool — one of `IDLE`, `PREPARING`, `PRINTING`, `PAUSED`, `COMPLETE`,
      `ERROR` (mirrors the six-state model stock uses on the Bambu side)
- [ ] Track `webhooks.state` so Klipper shutdown / firmware-restart shows
      as `ERROR` rather than "still printing"

### Material awareness
- [ ] Read Klipper `save_variables` (or a well-known gcode-macro variable
      set from `PRINT_START`) to pick up the current material. The tester
      already passes `MATERIAL=` into `PRINT_START` from his slicer, so
      the ingest side is free
- [ ] Configurable per-material rules in the portal: PLA → open above
      45 °C bed, ABS/ASA → stay closed for heat retention, PETG → open
      above 60 °C, etc. Ship sane defaults, let the user edit
- [ ] Bed-temperature thresholds move from hard-coded to per-material
      NVS-backed values (drop the current 35 / 45 °C constants)
- [ ] Fallback rule when material is unknown (probably: current
      bed-temperature hysteresis — what we do today)

### Smarter auto policy (`pv_policy`)
- [ ] Consume `pv_printer_state_t` — `COMPLETE` should keep the vent
      open while the chamber is still hot (residual-heat handling),
      `ERROR` should hold current state
- [ ] Chamber-temp override when `heater_generic chamber` is present
      (Voron-style enclosures)
- [ ] Manual-mode target survives reboot (currently reset on power cycle)

### Portal surfacing
- [ ] Home tab shows printer state (with the six-state label), material,
      progress %, chamber temp when available
- [ ] Printer tab: material-rule editor, threshold sliders
- [ ] Log/diagnostic tab that mirrors what `openvent-diag` sees — the
      hall raw + state stream, motor drive events, Moonraker connection
      health

### Verification before tagging 0.3.0
- [ ] All of the above tested on local devkit for logic correctness
- [ ] Full print cycle run on tester's real hardware — PLA and ABS at
      minimum, ideally with a paused / resumed print thrown in
- [ ] `openvent-diag` capture across a whole print, shared for review

## Phase 3 — Hall Sensor Calibration Parity

Stock does something we don't: per-boot ADC calibration on the hall
sensors, then interprets thresholds in millivolts rather than raw counts.
Our first attempt at this in v0.2.4 broke real hardware (LEDs went red,
device hung) — probably because the calibration pass raced with the
WS2812 output on the shared ADC/GPIO domain. Reverted in v0.2.5.

Not scheduled for a specific release — needs enough Ghidra time to
understand *what* stock actually does before we try again.

- [ ] Re-audit `adc_cali_raw_to_voltage` call sites in the stock binary:
      when is calibration performed (once at boot? on demand?), which
      channels are calibrated, and what's the exact scheme (line-fitting
      vs curve-fitting)
- [ ] Identify why our v0.2.4 attempt collided with the LED strip: shared
      RTC/ADC block? contention on GPIO 4/14 (ADC2)?
- [ ] Document threshold values in millivolts once we can convert
      OldGuy's captured raw values through the calibration curve
- [ ] Re-attempt calibration in a way that provably doesn't touch the
      WS2812 pins during startup
- [ ] Ship as a firmware-transparent change: user shouldn't need to
      recalibrate manually, and existing configs should still work

## Phase 4 — RGB Lighting (deferred)

WS2812 status lighting is the biggest gap vs stock. Out of scope until
the Moonraker + calibration work has landed and stabilized. Rough sketch
of what's needed when we do get here:

- WS2812 RMT driver, strip-count auto-detect via GPIO 35 ADC
- Simple mode: 7 canned effects with color/brightness/speed sliders
- Advanced mode: per-state color mapping (six printer states)
- Warning override (flash red on printer error)
- "Follow printer" / "Follow exhaust vent" / "Reverse direction" toggles

## Phase 5 — Extras & Polish

- [ ] Print progress on LEDs (once RGB lands)
- [ ] Klipper macro integration — expose HTTP endpoints so gcode macros
      can explicitly command vent open/close and RGB effects
- [ ] Home Assistant / MQTT bridge (optional fallback for non-Moonraker
      setups)
- [ ] Simplified Chinese portal translation (stock feature; low priority)

## Architecture

Firmware is split into standalone ESP-IDF components under
`firmware/components/`. `app_main` is a thin orchestrator: boot each
service in order, register the button callback, mirror policy mode to
the LED.

```
                    ┌─────────────────────────────┐
    printer ─── ws ──┤  pv_moonraker  (WS client)  │
                     │  state, temps, material     │
                     └──────────┬──────────────────┘
                                │ status
                                ▼
    button ──►  pv_button ──►  pv_policy ──►  pv_motor  ──► 4x DC motor
                     │        (auto/manual)      │           + hall ADC
                     │              │            │
                     ▼              ▼            │
               pv_status_led    pv_portal ◄──────┘  (web UI in both AP + STA)
               (GPIO 27)             │
                                     ▼
                                  pv_wifi (STA + AP fallback)

    pv_board = pin definitions shared by every component
```

Every long-lived component owns its own FreeRTOS task and exposes a
thread-safe API. Shared state (WiFi/Moonraker/policy) lives in the
`app_nvs` NVS namespace so it survives reboots and is compatible with
the stock firmware's layout.
