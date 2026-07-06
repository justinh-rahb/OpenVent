# PandaVent-Klipper

Custom firmware for the [Bigtreetech Panda Vent](https://github.com/bigtreetech/Panda-Vent) that replaces the stock Bambu Lab integration with [Moonraker](https://github.com/Arksine/moonraker)/[Klipper](https://www.klipper3d.org/) support.

## What is this?

The Panda Vent is a smart vent riser for enclosed 3D printers. The stock firmware only works with Bambu Lab printers via their proprietary MQTT protocol. This project replaces that firmware so the hardware works with any Klipper-based printer via Moonraker's API.

## Features (Planned)

- **Automatic vent control** — opens/closes based on bed temperature and print status
- **Captive portal WiFi setup** — same UX as the stock firmware
- **Moonraker integration** — connects via WebSocket for real-time printer status
- **Physical button control** — auto/manual mode toggle, manual vent override
- **Web configuration UI** — Moonraker connection, vent settings
- **OTA firmware updates** — flash new firmware from the web UI
- **RGB LED status** (Phase 2) — printer state visualization via WS2812 strips

## Documentation

- [Hardware Analysis](docs/HARDWARE_ANALYSIS.md) — reverse-engineered GPIO pinout and hardware details
- [Roadmap](docs/ROADMAP.md) — development phases and architecture

## Hardware

- **Board**: Bigtreetech Panda Vent (ESP32 Xtensa dual-core)
- **Motor**: DC motor with forward/reverse PWM via LEDC, hall sensor position feedback
- **LEDs**: WS2812 addressable strips via RMT (16 or 27 LEDs, auto-detected)
- **Button**: GPIO 12 (illuminated), GPIO 0 (BOOT/factory reset)

## Status

🚧 **Early development** — reverse engineering hardware, building firmware from scratch.

## License

TBD
