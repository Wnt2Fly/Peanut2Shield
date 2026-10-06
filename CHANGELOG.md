# Changelog

All notable firmware and documentation changes for Peanut2Shield.
The firmware version lives in `CFG_FIRMWARE_VERSION` in `src/config.h`.

> **Upgrading clears pairing once.** The first boot after flashing a *different* version erases NVS, so you re-pair the Shield and the TiVo remote one time. Re-flashing the *same* version with PlatformIO keeps pairing; flashing a prebuilt single-file image always clears it.

---

## v1.21 — 2026-10-06

### Fixed
- **Long-press mode: every tap acted like a hold.** A tap on OK opened TiviMate's side menu, a single Back gave the guide with a black picture, and Back sometimes did nothing. Key releases were sent from the main loop, so whenever the loop stalled the button stayed down on the Shield. Releases now run from their own timer and no longer depend on the main loop. This applies to both builds: the Esc/F-key/Home short taps now use the timer too.
- **LED freezing on solid white/purple and no longer flashing on button presses.** The Bluetooth callback and the main loop could both drive the LED at the same moment. Only the main loop writes to the LED now; button presses just request a flash.

### Added
- **Safety release in long-press mode** (`CFG_LONG_PRESS_MAX_HOLD_MS`, 3 s): a held key is always released on the Shield after 3 s, even if the remote's key-up is lost.

## v1.20 — 2026-10-06

### Added
- **Prebuilt firmware in the repo** (`firmware/`): `Peanut2Shield-v1.20-standard.bin` and `Peanut2Shield-v1.20-longpress.bin`. Each is a complete single-file image that flashes at address `0x0` from Chrome/Edge ([esptool-js](https://espressif.github.io/esptool-js/)) on Windows, Mac, or Linux — no PlatformIO needed.
- **`make-release.ps1`** rebuilds both images from source.
- **Optional long-press pass-through** (`CFG_LONG_PRESS`, **off by default**; the `longpress` image has it on). Buttons stay held on the Shield for as long as they are held on the remote, so apps like TiviMate see long-presses (long Back to jump back to full screen, long OK for "Play in external player" / "Add to favorites"). Back is sent as Android Back instead of Esc in this mode. F-keys and Home keep the short tap. With the default `0`, button behaviour is unchanged from v1.18. Can also be set at build time with `-DCFG_LONG_PRESS=1`.

### Fixed
- **Orange double-flash while the remote still works (after Shield sleep/wake).** A TiVo disconnect scheduled a reconnect for 3 s later, but the main loop also reconnected immediately. When the timer then fired, a second connection was started on top of the live one: the old link kept forwarding keys, while the bridge marked the TiVo "not ready" and retried every couple of seconds. Now:
  - the scheduled 3 s reconnect is the only reconnect path,
  - a reconnect is skipped if the TiVo is already connected (HID setup is re-run instead),
  - a disconnect from an old, replaced client no longer marks the current link not-ready.

## v1.18 — 2026-09-10

### Fixed
- **Stuck orange double-flash after a power outage.** When power returned, the Shield and TiVo reconnected at the same time and the v1.17 soft refresh tore down the TiVo link mid-recovery. The remote kept working but the LED stayed on orange double-flash until RESET. Soft refresh now runs only after the Shield drops **mid-session** and comes back — never on cold boot or power restoration.
- **Self-heal for a "connected but not ready" TiVo link.** If the TiVo is connected with a trusted bond but the ready flag was lost, HID setup re-runs automatically instead of waiting for a RESET.

## v1.17 — 2026-09-08

### Fixed
- **Back (and other keys) dropping out until re-pair.** The TiVo link could stay "connected" while one of its two HID report channels stopped delivering, so some keys never reached the bridge (no white flash). Re-pairing fixed it only temporarily.
  - **Soft refresh:** after the Shield drops and reconnects, the bridge disconnects and reconnects the TiVo remote using the **saved bond** — no forget, no pairing mode.
  - **Periodic re-subscribe:** every 5 minutes the bridge re-enables notifications on all TiVo report channels.
  - **Subscribe check:** if fewer than two report channels come up after connect, a soft refresh is scheduled automatically.
- New tuning constants in `config.h`: `CFG_TIVO_SOFT_REFRESH_COOLDOWN_MS`, `CFG_TIVO_SOFT_REFRESH_AFTER_SHIELD_MS`, `CFG_TIVO_RESUBSCRIBE_MS`, `CFG_TIVO_MIN_SUBSCRIBED_REPORTS`.

## v1.16 — 2026-08-14

### Fixed
- **Extra BOOT press needed after 3 or 4 presses.** The confirmation flashes were saved as the LED's base pattern, so they looped and blocked the normal pairing LED until another press. Confirmation flashes are now one-shot and the LED returns to the pairing pattern on its own.
- Forget-TiVo (4 presses / serial `t`) now confirms with **orange** flashes (was purple).

### Changed
- **Steady green "ready" LED is half as bright** (`CFG_LED_COLOR_READY` = `0, 128, 0`).

## v1.15 — 2026-08-14

### Fixed
- After the BOOT confirmation flashes, the LED returns to the base pattern instead of staying on the confirm animation.

## v1.14

### Fixed
- 3-press Shield forget no longer leaves the LED on a repeating purple double-flash; it shows a one-time confirm, then the normal pattern.

## v1.13

### Changed
- **BOOT button uses short presses** instead of hold timings: **3 = forget Shield, 4 = forget TiVo, 5 = factory reset** (wait ~1 s after the last press). Each press gives a brief white tick.

## v1.12

### Added
- Serial commands over the USB monitor: `t` forget TiVo, `s` forget Shield, `f` factory reset, `i` status, `h` help.

## v1.11 — 2026-08-04

### Fixed
- Serial monitor showed nothing: logging was gated on `(bool)Serial`, which often stays false on ESP32-S3 USB. Logs now print whenever `setup()` has finished.

## v1.10 — 2026-08-04

### Fixed
- **Purple blink → solid orange with no Shield resync after unplug/replug.** A TiVo reconnect paused advertising, so the Shield never found the bridge. Advertising now continues (slow intervals) while the Shield is not linked.
- A Shield address stored in NVS is no longer treated as "Shield ready" on cold boot.
- Boot wait for the Shield before starting TiVo work raised to 30 s (`CFG_SHIELD_RECONNECT_BOOT_MS`).

### Removed
- USB-stick update kit (`usb-drive/`, `pack-usb-drive.*`). It could flash stale firmware. Use PlatformIO (and `flash-recover.bat` for crash loops).

## v1.08 – v1.09 — 2026-08-01

### Changed
- **TiVo Power is no longer forwarded over BLE** (`CFG_IGNORE_TIVO_POWER_BLE=1`) — it was sleeping/waking the Shield. Use the remote's IR Power.
- **Volume/Mute are no longer forwarded over BLE** (`CFG_IGNORE_TIVO_VOLUME_BLE=1`) — they double-adjusted with IR/CEC.
- Docs: prefer wall USB power; avoid the Shield's USB port (USB serial hang → solid purple).

## v1.07 — 2026-07-13

### Fixed
- **TiVo "lost pairing" after a short drop.** A disconnect inside the 5 s confirm window now clears only **untrusted** bonds; trusted bonds reconnect instead of being wiped.

## v1.04 – v1.06

### Fixed
- **Solid yellow / solid purple boot hang** on USB hosts that enumerate the serial port but never read it (PC or Shield USB). Added `boot_early.cpp`, `Serial.setTxTimeoutMs(0)`, silenced ESP-IDF logs, and deferred serial logging until `setup()` finishes.

## v1.03 — 2026-06

### Fixed
- **Crash loop after firmware update** (`Config struct mismatch` / `ESP_ERR_NO_MEM`). NVS is erased on first boot after a firmware version change so stale Bluetooth config cannot crash the stack.

## v1.02 — 2026-06-05

### Added
- Shield link debug logging (`CFG_SHIELD_DEBUG`) and a boot reconnect window so the Shield reconnects before TiVo work starts.

## Earlier — 2026-05 / 2026-06

- Initial ESP32-C3 bridge, then migration to the **Waveshare ESP32-S3-Zero** with RGB status LED, BOOT button bond management, and WiFi removed.
- Duplicate-keypress fixes for the TiVo's dual consumer report characteristics.
- 3D-printed case, TiVo IR programming codes, flash recovery script.
