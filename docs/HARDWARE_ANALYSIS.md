# Panda Vent Hardware Analysis

> Reverse-engineered from the stock firmware binary (`panda_vent_v1.0.0.bin`)
> and official BTT documentation.

## ESP32 Module

The firmware binary confirms this is a **classic ESP32 (Xtensa dual-core LX6)** — NOT an S2/S3/C3.

Evidence from binary strings:
- HAL paths: `hal/esp32/include/hal/gpio_ll.h`, `hal/esp32/include/hal/adc_ll.h`
- Toolchain: `xtensa-esp-elf` (not `riscv32-esp-elf`)
- Port code: `port/esp32/rtc_time.c`, `port/soc/esp32/clk.c`
- Built with **ESP-IDF** (not Arduino)

## GPIO Pin Map

### Confirmed (from documentation)

| Function | GPIO | Peripheral | Source |
|----------|------|-----------|--------|
| **User Button (with LED)** | GPIO 12 | GPIO input + LED output | BTT User Manual |
| **BOOT Button** | GPIO 0 | GPIO input (factory reset) | BTT User Manual |

### Confirmed (from firmware binary strings)

| Function | GPIO / Channel | Peripheral | Evidence |
|----------|----------------|-----------|----------|
| **LED Strip Detection** | ADC1_CHANNEL_7 (GPIO 35) | ADC oneshot | `adc_oneshot_config_channel(hall_adc_handle, ADC1_CHANNEL_7, &chan_config)` |
| **Hall Sensor Group 1** | GROUP1_ADC_CHANNEL | ADC oneshot | `adc_oneshot_config_channel(hall_adc_handle, GROUP1_ADC_CHANNEL, &chan_config)` |
| **Hall Sensor Group 2** | GROUP2_ADC_CHANNEL | ADC oneshot | `adc_oneshot_config_channel(hall_adc_handle, GROUP2_ADC_CHANNEL, &chan_config)` |
| **Hall Sensor Group 3** | GROUP3_ADC_CHANNEL | ADC oneshot | `adc_oneshot_config_channel(hall_adc_handle, GROUP3_ADC_CHANNEL, &chan_config)` |
| **Hall Sensor Group 4** | GROUP4_ADC_CHANNEL | ADC oneshot | `adc_oneshot_config_channel(hall_adc_handle, GROUP4_ADC_CHANNEL, &chan_config)` |
| **Motor Forward PWM** | Unknown GPIO | LEDC channel | `ledc_channel_config(&fwd_chan)` |
| **Motor Reverse PWM** | Unknown GPIO | LEDC channel | `ledc_channel_config(&rev_chan)` |
| **WS2812 RGB Strip(s)** | Unknown GPIO(s) | RMT TX | `rmt_new_tx_channel`, `rgb_channels[i]` (multi-channel array) |

### ESP32 ADC1 Channel → GPIO Reference

| ADC1 Channel | GPIO |
|-------------|------|
| CH0 | GPIO 36 (SVP) |
| CH1 | GPIO 37 |
| CH2 | GPIO 38 |
| CH3 | GPIO 39 (SVN) |
| CH4 | GPIO 32 |
| CH5 | GPIO 33 |
| CH6 | GPIO 34 |
| CH7 | GPIO 35 |

Note: GPIO 34-39 are input-only on the ESP32.

## Motor Subsystem

Source file paths found in binary:
- `./main/motor/motor.c` — motor control logic
- `./main/motor/motor_adc.c` — hall sensor reading

Key details:
- **4 motor channel groups** (GROUP 1-4), each with independent hall sensor
- **Forward/reverse PWM** via ESP32 LEDC peripheral (`fwd_chan` / `rev_chan`)
- **Gradual startup** (soft-start ramp to prevent mechanical shock)
- Functions: `motor_pwm_init`, `motor_ledc_timer_init`, `hall_adc_init`, `hall_get_state`
- LEDC functions used: `ledc_timer_config`, `ledc_channel_config`, `ledc_set_duty`, `ledc_update_duty`, `ledc_set_fade_with_time`, `ledc_fade_start`

## RGB LED Subsystem

Source file paths found in binary:
- `./main/rgb/app_rgb.c` — LED strip control
- `./main/rgb/app_rgb_effect.c` — lighting effects engine

Key details:
- Uses **RMT peripheral** for WS2812 timing (standard ESP-IDF approach)
- Supports **multiple RMT channels**: `rgb_channels[i].channel`, `rgb_channels[i].encoder`
- Auto-detects 1 strip (16 LEDs) vs 2 strips (27 LEDs) via ADC on GPIO 35
- 7 effect modes: static, breathing, strobing, wave, marquee, color cycle, rainbow
- Functions: `rgb_init`, `rgb_light_mode`, `rgb_switch`, `rgb_sundry`, `sys_rgb_mode`, `rmt_new_led_strip_encoder`, `rmt_transmit`

## Communication Subsystem (Stock Firmware)

- **WiFi**: STA + AP mode, mDNS hostname (`PandaVent.local`)
- **MQTT**: `bambu_mqtt` module, local connection to Bambu printer
- **UDP Discovery**: `bambu_udp` — SSDP over UDP to discover Bambu printers
- **Web UI**: ESP HTTP Server (`httpd`) with WebSocket support
- **OTA**: HTTPS OTA via web upload
- **Hotspot**: Default SSID `Panda_Vent_XXXX`, password `987654321`, IP `192.168.254.1`

### MQTT Fields Parsed from Bambu Printers

| Field | Description |
|-------|-------------|
| `gcode_state` | Current print state |
| `stg_cur` | Current stage |
| `nozzle_temper` | Nozzle temperature |
| `bed_temper` | Bed temperature |
| `layer_num` | Current layer number |
| `print_error` | Error code |

Push command sent to Bambu printer:
```json
{"pushing": {"sequence_id": "0", "command": "pushall"}}
```

## NVS Storage Keys

Found in binary strings:

| Key | Purpose |
|-----|---------|
| `ssid` | WiFi SSID |
| `password` | WiFi password |
| `access_code` | Bambu printer access code |
| `ap.ssid` | AP mode SSID |
| `language` | UI language |
| `current_light_mode` | Active lighting mode |
| `rgb_light_mode` | RGB mode setting |
| `sys_rgb_mode` | System RGB mode |
| `warn` | Warning state |
| `warning_hot_mode` | Temperature warning mode |
| `warning_overide` | Warning override flag |
| `warning_sw` | Warning switch |
| `set_hostname` | mDNS hostname |

## Firmware Build Info

- NVS namespace: `app_nvs`
- Device name format: `ESP32_%02x%02X%02X` (MAC-based)
- AP SSID prefix: `Panda_Vent_`
- Built with ESP-IDF, cross-compiled on Windows (`HOST-x86_64-w64-mingw32`)
