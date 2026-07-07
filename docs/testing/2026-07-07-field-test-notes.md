# Field Test Notes — 2026-07-07

**Participants:** OldGuyMeltsPlastic (tester), wildtang3nt (dev)
**Setup:** Panda Vent disconnected from P1S, custom firmware flashed to ESP32

## Summary

First live test of the custom firmware on real Panda Vent hardware. Flashing, AP mode, and the web UI all worked. Vent open/close control is unreliable — motors don't stop when they should, most likely a hall sensor tuning issue. Restoring OEM firmware afterward worked cleanly.

## Timeline

1. Flashed custom firmware; `OpenVent_XXXX` AP appeared, connected fine, web UI loaded (vents started closed).
2. Clicked "Open vent" — vents opened, but motors kept chugging for a few seconds after. Browser then lost the AP connection; had to manually reconnect.
3. Back in the UI: Manual mode was active (round mode button flashing, as expected per Biqu docs). "Close vent" button did nothing — expected in Manual mode.
4. AP dropped again and wouldn't reappear on the WiFi adapter. `/dev/ttyUSB0` was still up in PuTTY. Power-cycled the ESP32 via USB.
5. After power cycle: mode button flashing again (Manual), vents still open, AP back.
   - Short press of the round button (should close vents): motors spun briefly, nothing happened, vents stayed open.
   - Long press (3s, should set Auto): button light stopped flashing, but vents still didn't close.
   - Web UI still showed Manual mode after the long press; a second 3s press + UI refresh finally showed Auto mode. Vents still open.
6. Dev note: pressing the physical button always sets Manual mode and toggles the vent — the motors not stopping is likely a hall sensor calibration issue, not a mode issue.
7. Vents could not be closed via UI or button. Manually closing by hand with gentle pressure worked, but seal was looser than OEM firmware achieves.
8. Checked motors for damage (smell/smoke) — none found, no blue smoke.
9. As a sanity check, restored OEM (stock) firmware:
   - Some uncertainty about what state causes the AP to disappear.
   - Backup firmware flashed successfully via USB.
   - Once on WiFi, AP turned off automatically (expected/normal behavior).
10. Restore confirmed successful: USB-C disconnected, Bambu cable reconnected, built-in AP disabled (as set before testing), `pandavent.local` active, vents fully closed, system back to normal.

## Issues Found

- **Motors don't stop on cue** — after open/close commands, motors run longer than needed. Suspected root cause: hall sensor readings not tuned/calibrated correctly.
- **Vents wouldn't close via UI or hardware button** in this session, despite Manual/Auto mode switching working as documented.
- **AP instability** — the AP dropped twice and was slow/unreliable to reappear; unclear what firmware state triggers this.
- **UI feedback request** — tester asked for a dark mode (white background called out as hard on the eyes).

## What Worked

- Firmware flash process (USB) — smooth, both directions (custom → OEM restore).
- AP + captive web UI came up correctly on first flash.
- Auto/Manual mode indicator (flashing button) behaved per Biqu's documented spec.
- AP auto-disable once connected to home WiFi — working as intended, expected behavior on this firmware.
- OEM firmware restore — fully successful, vents closed properly, system back to normal.
- Mode-switch button to UI-reflected state delay — this lag is known/expected, not a bug.

## Dev Notes / Explanations (wildtang3nt)

- By design, the AP turns off once the device successfully connects to the configured WiFi network. Seeing the AP implies WiFi is either unavailable or not yet configured. This is expected behavior, not a bug.
- The delay between pressing the physical mode-switch button and the web UI reflecting the new mode is a known, expected behavior.
- Long-pressing the boot button (3+ seconds) triggers a factory reset.
- Pressing the physical user button sets Manual mode and opens/closes the vent.
- The motor-stall sound is likely the hall sensors not being tuned properly — motors aren't cutting off when they should.

## Next Steps

- Dev to iterate on the build with better motor-movement debugging (likely hall sensor calibration).
- Add dark mode to the web UI.
- Tester available for further build testing.
