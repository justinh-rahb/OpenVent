#pragma once

// Captive portal: DNS server that answers every A-record query with the AP
// gateway IP, plus an HTTP server that serves the WiFi setup page and posts
// submitted credentials to a caller-supplied callback. Call after WiFi is
// running in AP mode.

#include "esp_err.h"

// Invoked from the HTTP request handler when the user submits the form.
// Typically stores the credentials to NVS and reboots into STA mode; the
// implementation is expected NOT to return (or to return quickly enough for
// the HTTP response to flush).
typedef esp_err_t (*pv_portal_save_cb_t)(const char *ssid, const char *password);

esp_err_t pv_portal_start(pv_portal_save_cb_t save_cb);
esp_err_t pv_portal_stop(void);
