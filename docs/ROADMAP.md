# PandaVent-Klipper Roadmap

Custom firmware for the Bigtreetech Panda Vent hardware that replaces the
stock Bambu Lab MQTT integration with Moonraker/Klipper support.

## Goals

- Repurpose stock Panda Vent hardware for Klipper-based printers
- Automatically open/close the vent based on printer status (bed temperature)
- Provide a captive portal AP for WiFi setup (like stock firmware)
- Simple web UI for Moonraker connection settings
- OTA firmware updates via web UI

## Phase 1 — MVP (Current Focus)

### Vent Control via Moonraker
- [ ] Connect to Moonraker via WebSocket or HTTP polling
- [ ] Subscribe to `print_stats` and `heater_bed` objects
- [ ] Open vent when bed is heated / printing is active
- [ ] Close vent when bed cools down / printer is idle
- [ ] Auto/manual mode toggle via physical button (GPIO 12)
- [ ] Long-press button to switch between auto and manual mode

### WiFi & Captive Portal
- [ ] AP mode on first boot (captive portal with DNS redirect)
- [ ] Web page: scan WiFi networks, select SSID, enter password
- [ ] Store WiFi credentials in NVS
- [ ] Auto-reconnect to saved WiFi on boot

### Moonraker Configuration
- [ ] Web page: enter Moonraker IP/hostname and port
- [ ] Optional API key field
- [ ] mDNS discovery of `_moonraker._tcp` services
- [ ] Store config in NVS
- [ ] Connection status indicator on web UI

### Hardware Drivers
- [ ] Confirm all GPIO pin assignments (Ghidra disassembly)
- [ ] Motor driver: LEDC PWM forward/reverse with soft-start
- [ ] Hall sensor reading for vent position feedback
- [ ] Button handler: debounce, single-click, long-press
- [ ] Status LED on GPIO 12 button (solid = auto, blink = manual)

## Phase 2 — RGB & Effects

- [ ] WS2812 LED strip driver via RMT
- [ ] Auto-detect strip count (1 or 2) via ADC on GPIO 35
- [ ] Map printer states to LED colors/effects
- [ ] Web UI for RGB configuration
- [ ] Temperature-based color gradients

## Phase 3 — Polish

- [ ] OTA firmware update via web UI
- [ ] Factory reset via BOOT button long-press
- [ ] mDNS hostname configuration
- [ ] Print progress on LEDs (percentage bar)
- [ ] Klipper macro integration (allow macros to control vent/LEDs)
- [ ] Home Assistant / MQTT bridge (optional)

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Panda Vent (ESP32)                              │
│                                                  │
│  ┌──────────┐  ┌──────────────┐  ┌───────────┐  │
│  │ Captive  │  │  Moonraker   │  │   State   │  │
│  │ Portal   │  │   Client     │  │  Machine  │  │
│  │ (httpd)  │  │  (websocket) │  │           │  │
│  └────┬─────┘  └──────┬───────┘  └─────┬─────┘  │
│       │               │                │         │
│       │               ▼                ▼         │
│       │         ┌──────────┐    ┌───────────┐    │
│       │         │   NVS    │    │  Motor    │    │
│       │         │  Config  │    │  Driver   │    │
│       │         └──────────┘    │  (LEDC)   │    │
│       │                         └───────────┘    │
│       ▼                                          │
│  ┌──────────┐                   ┌───────────┐    │
│  │  WiFi    │                   │  RGB LEDs │    │
│  │  (STA+AP)│                   │  (RMT)    │    │
│  └──────────┘                   └───────────┘    │
└──────────────────────────────────────────────────┘
         │                              
         ▼                              
┌──────────────────┐                    
│  Klipper Host    │                    
│  ┌────────────┐  │                    
│  │ Moonraker  │  │                    
│  └────────────┘  │                    
│  ┌────────────┐  │                    
│  │  Klipper   │  │                    
│  └────────────┘  │                    
└──────────────────┘                    
```

## State Machine

```
                    ┌─────────┐
          ┌────────►│  IDLE   │◄────────┐
          │         │vent shut│         │
          │         └────┬────┘         │
          │              │              │
     bed cools      bed heats      error/cancel
     below thr.     above thr.          │
          │              │              │
          │              ▼              │
          │         ┌─────────┐         │
          ├─────────│PRINTING │─────────┤
          │         │vent open│         │
          │         └────┬────┘         │
          │              │              │
          │           paused            │
          │              │              │
          │              ▼              │
          │         ┌─────────┐         │
          └─────────│ PAUSED  │─────────┘
                    │vent hold│
                    └─────────┘
```

## Moonraker Status Mapping

| Moonraker `print_stats.state` | Vent Action | LED (Phase 2) |
|-------------------------------|-------------|---------------|
| `"standby"` | Closed | White |
| `"printing"` | Open (auto) | Rainbow |
| `"paused"` | Hold position | White |
| `"complete"` | Open (cool-down timer) | Green |
| `"error"` / `"cancelled"` | Closed | Red |
| Klipper not ready | Closed | Breathing blue |

Bed temperature is the primary trigger: vent opens when `heater_bed.temperature`
exceeds a configurable threshold (default: 40°C), closes when it drops below.
