# OpenVent

Custom firmware for the [Bigtreetech Panda Vent](https://github.com/bigtreetech/Panda-Vent) that replaces the stock Bambu Lab integration with [Moonraker](https://github.com/Arksine/moonraker)/[Klipper](https://www.klipper3d.org/) support.

## What is this?

The Panda Vent is a smart vent riser for enclosed 3D printers. The stock firmware only works with Bambu Lab printers via their proprietary MQTT protocol. OpenVent replaces that firmware so the hardware works with any Klipper-based printer via Moonraker's API.

## Features

Working today (v0.2.6):

- **Automatic vent control** — opens/closes based on print state + bed-temp hysteresis
- **Captive portal WiFi setup** — same UX as the stock firmware
- **Moonraker integration** — WebSocket connection to `print_stats` + `heater_bed`
- **Physical button control** — auto/manual mode toggle, manual vent override
- **Web configuration UI** — tabbed layout for WiFi, Moonraker, and system
- **OTA firmware updates** — flash new firmware from the web UI

Coming in the 0.3.x series:

- **Richer Moonraker ingest** — full six-state printer model, live push updates, chamber temp
- **Material-aware auto mode** — PLA opens for cooling, ABS/ASA seals for heat retention, read from `PRINT_START`
- **Configurable thresholds** — per-material bed-temp rules editable in the portal

Deferred:

- **Per-boot hall calibration** — stock does this in millivolts; our first attempt broke real hardware. Needs more Ghidra time before we retry
- **RGB LED status** — WS2812 driver + effects engine, planned for after the Moonraker + calibration work stabilizes

## Documentation

- [Hardware Analysis](docs/HARDWARE_ANALYSIS.md) — reverse-engineered GPIO pinout and hardware details
- [Roadmap](docs/ROADMAP.md) — development phases and architecture

## Hardware

- **Kit contents**: 1 mainboard + several motorized vent modules + LED boards. Each vent module has one motor, one hall sensor, and identical 3-pin JST connectors on both ends — so multiple modules chain together
- **Board**: Bigtreetech Panda Vent (ESP32 Xtensa dual-core LX6)
- **Motors**: up to 4 independent DC motors across two mainboard chains, each driven forward/reverse via LEDC PWM at 30 kHz with hall-sensor position feedback
- **LEDs**: WS2812 addressable strips via RMT — GPIO 14 and GPIO 4, one per chain
- **User button**: switch on GPIO 12, illumination LED on GPIO 27 (off = auto, blink = manual)
- **BOOT button**: GPIO 0 (long-press = factory reset)
- **Hardware auto-detect**: single ADC on GPIO 35 picks between "all chains populated" (4 motors), "one chain" (2 motors), and "nothing" — hot-plug supported

Full pin map + provenance: [docs/HARDWARE_ANALYSIS.md](docs/HARDWARE_ANALYSIS.md).

## ⚠ Back up your stock firmware first

Before flashing OpenVent, **dump the whole flash** so you have a way back —
BTT doesn't publish source or release binaries for the Panda Vent, so if you
lose the stock image there's no official way to recover it. The included
[`scripts/openvent`](scripts/openvent) helper wraps this up:

```
scripts/openvent backup                 # → stock-panda-vent-backup.bin
scripts/openvent install v0.2.0         # download + flash a release
scripts/openvent restore stock-panda-vent-backup.bin   # roll back
```

(All three take an optional `-p /dev/tty.usbserial-XXXX` if the default
autodetect doesn't find the right port.)

## Status

**v0.2.6 is the first stable proof-of-concept release.** 2026-07-10 field
test on tester OldGuyMeltsPlastic's retail 2-vent kit: 10× consecutive
open/close cycles, no ESP crash, motor stops cleanly on each arrival.
See [ROADMAP.md](docs/ROADMAP.md) for what's shipped and what's next.

- ✅ Firmware flashes on real Panda Vent hardware; `openvent` script for
  backup / restore / install works end-to-end
- ✅ WiFi station + AP fallback, mDNS `OpenVent.local`, captive portal
- ✅ Portal: tabbed UI, live status, WiFi setup, Moonraker config, OTA upload, factory reset, dark mode
- ✅ Moonraker WebSocket client with `print_stats` + `heater_bed` subscriptions
- ✅ Motors drive both directions and reliably stop at endpoints
- ⏳ 0.3.0 in progress: full six-state printer model + material-aware auto
- ⬜ Deferred: WS2812 RGB, per-boot hall calibration

[![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/wildtang3nt)

## License

[MIT](LICENSE) © Justin Hayes
