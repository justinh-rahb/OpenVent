#include "pv_portal.h"
#include "pv_dns.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"

#include <string.h>

static const char *TAG = "pv_portal";

static httpd_handle_t       s_httpd   = NULL;
static pv_portal_save_cb_t  s_save_cb = NULL;

// ---------- HTML ----------

static const char *SETUP_HTML =
"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>PandaVent Setup</title>"
"<style>"
"body{font-family:sans-serif;max-width:420px;margin:2em auto;padding:0 1em}"
"h1{font-size:1.4em}label{display:block;margin:1em 0 .25em}"
"input,button{width:100%;padding:.6em;font-size:1em;box-sizing:border-box}"
"button{margin-top:1.5em;background:#222;color:#fff;border:0;border-radius:4px}"
"</style></head><body>"
"<h1>PandaVent WiFi setup</h1>"
"<form method=\"POST\" action=\"/save\">"
"<label>Network SSID</label>"
"<input name=\"ssid\" required maxlength=\"32\">"
"<label>Password</label>"
"<input name=\"password\" type=\"password\" maxlength=\"64\">"
"<button type=\"submit\">Save &amp; reboot</button>"
"</form></body></html>";

static const char *DONE_HTML =
"<!DOCTYPE html><html><body>"
"<h1>Saved. Rebooting…</h1>"
"<p>PandaVent will disconnect from this network and try to join yours.</p>"
"</body></html>";

// ---------- helpers ----------

// Extract a value for `key` from a url-encoded form body. Writes into `out`
// and NUL-terminates. Returns 0 on success, -1 if not found.
static int form_get(const char *body, const char *key, char *out, size_t out_sz)
{
    size_t klen = strlen(key);
    const char *p = body;
    while (*p) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            size_t o = 0;
            while (*p && *p != '&' && o + 1 < out_sz) {
                char c = *p++;
                if (c == '+') {
                    out[o++] = ' ';
                } else if (c == '%' && p[0] && p[1]) {
                    char hex[3] = { p[0], p[1], 0 };
                    out[o++] = (char)strtol(hex, NULL, 16);
                    p += 2;
                } else {
                    out[o++] = c;
                }
            }
            out[o] = '\0';
            return 0;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    return -1;
}

// ---------- handlers ----------

static esp_err_t setup_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, SETUP_HTML);
}

static esp_err_t save_post(httpd_req_t *req)
{
    char body[256];
    int total = req->content_len;
    if (total <= 0 || total >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body too long");
        return ESP_OK;
    }
    int off = 0;
    while (off < total) {
        int n = httpd_req_recv(req, body + off, total - off);
        if (n <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "recv failed");
            return ESP_OK;
        }
        off += n;
    }
    body[off] = '\0';

    char ssid[33] = {0};
    char pass[65] = {0};
    if (form_get(body, "ssid", ssid, sizeof(ssid)) != 0 || ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
        return ESP_OK;
    }
    form_get(body, "password", pass, sizeof(pass));   // ok if missing

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, DONE_HTML);

    if (s_save_cb) s_save_cb(ssid, pass);   // typically reboots; won't return
    return ESP_OK;
}

// Catch-all: send a 302 back to the setup page so captive-portal detection
// probes (Apple/Google/Microsoft) trigger the "Sign in to network" banner.
static esp_err_t captive_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ---------- start / stop ----------

static uint32_t get_ap_gateway_ip(void)
{
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap == NULL) return 0;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(ap, &info) != ESP_OK) return 0;
    return info.gw.addr;   // already network byte order
}

esp_err_t pv_portal_start(pv_portal_save_cb_t save_cb)
{
    if (s_httpd != NULL) return ESP_ERR_INVALID_STATE;
    s_save_cb = save_cb;

    uint32_t ip = get_ap_gateway_ip();
    if (ip == 0) {
        ESP_LOGE(TAG, "no AP netif; call after esp_wifi_start in AP mode");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = pv_dns_start(ip);
    if (err != ESP_OK) return err;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    err = httpd_start(&s_httpd, &cfg);
    if (err != ESP_OK) return err;

    httpd_uri_t get_root  = { .uri = "/",     .method = HTTP_GET,  .handler = setup_get };
    httpd_uri_t post_save = { .uri = "/save", .method = HTTP_POST, .handler = save_post };
    httpd_uri_t catchall  = { .uri = "/*",    .method = HTTP_GET,  .handler = captive_redirect };
    httpd_register_uri_handler(s_httpd, &get_root);
    httpd_register_uri_handler(s_httpd, &post_save);
    httpd_register_uri_handler(s_httpd, &catchall);

    ESP_LOGI(TAG, "portal up on http://" IPSTR, IP2STR((esp_ip4_addr_t *)&ip));
    return ESP_OK;
}

esp_err_t pv_portal_stop(void)
{
    pv_dns_stop();
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    s_save_cb = NULL;
    return ESP_OK;
}
