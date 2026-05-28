# TiVo → Nvidia Shield BLE HID Translator

An ESP32-C3 firmware that bridges a **TiVo Stream 4K remote** to an **Nvidia Shield TV** over Bluetooth LE — with a built-in web admin panel for live remapping, WiFi config, and device management.

---

## What it does

```
TiVo Remote  ──BLE HID──►  ESP32-C3  ──BLE HID──►  Nvidia Shield TV
              (Central)   Translator   (Peripheral)
```

The ESP32-C3 simultaneously acts as:

- **BLE Central** — connects to the TiVo remote as a HID host and receives button reports
- **BLE Peripheral** — advertises as a HID keyboard + consumer control device to the Shield

When a button is pressed on the TiVo remote, the firmware translates it (if needed) and forwards it to the Shield in real time.

---

## Hardware

| Part | Detail |
|------|--------|
| MCU | ESP32-C3 Super Mini |
| LED | Built-in GPIO 8 (active-low, flashes on each forwarded keypress) |
| Boot button | GPIO 9 — hold 3 s to re-enable the WiFi AP if it has timed out |

---

## Default key translation table

| TiVo button | Usage code | Forwarded as |
|-------------|-----------|--------------|
| TiVo | `0x003D` | Keyboard F4 (`0x3D`) |
| Live TV | `0x003E` | Keyboard F7 (`0x40`) |
| Guide | `0x008D` | Keyboard F8 (`0x41`) |
| Info | `0x0209` | Keyboard F9 (`0x42`) |
| Skip | `0xCE00` | Keyboard F10 (`0x43`) |
| Netflix | `0x01C8` | Keyboard F11 (`0x44`) |
| Back | `0x0224` | Keyboard ESC (`0x29`) |
| Home | `0x0223` | Consumer AC Home (pass-through) |
| OK / Select | `0x0041` | Consumer Menu Pick (pass-through) |
| All other buttons | — | Consumer control pass-through |

All mappings are fully remappable from the web UI and persist across reboots.

---

## Web admin panel

Connect to the **TiVoTranslator** WiFi AP (password: `tivotivo`) and open **http://192.168.4.1**.

The panel has three tabs:

### WiFi
- Connect the device to your home network (STA mode) — the AP stays on as a fallback
- View current IP addresses

### Button Map
- See every known TiVo button and what it maps to
- Change any mapping (KEY with named dropdown, or CSM hex value)
- Reset any mapping back to firmware default
- Changes take effect immediately and survive reboots (stored in NVS)

### Devices
- View TiVo remote bond status and address
- View Nvidia Shield bond status and address
- Re-pair TiVo remote (forget bond → puts device back into scan mode)
- Re-pair Shield (forget bond → restarts advertising)
- WiFi AP Timeout — set minutes of HTTP inactivity before the AP shuts off (0 = always on)
- BLE TX Power — adjust transmit power (N12 dBm … P9 dBm, default P0)

---

## Re-pairing procedure

### TiVo remote
1. In the web UI → **Devices** tab → click **Forget & Re-pair TiVo**
2. On the remote hold **TiVo + Back** until the light flashes

### Nvidia Shield
1. On the Shield go to **Settings → Remote & Accessories → Add Accessory**
2. In the web UI → **Devices** tab → click **Forget & Re-pair Shield**
3. The ESP32 deletes the old bond and restarts advertising; pair from the Shield

> **If the Shield shows "paired" but nothing happens:** forget the device on the Shield first, then use the web UI button — always clear both sides together.

---

## Building & flashing

### Requirements

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- ESP32-C3 board connected via USB

### Build

```bash
pio run
```

### Flash

```bash
pio run --target upload
```

### Monitor

```bash
pio device monitor --baud 115200
```

> On Windows, stop the monitor before flashing (PlatformIO cannot upload while the serial port is held open).

---

## Project structure

```
├── platformio.ini          # Board, platform, library dependencies
└── src/
    ├── ble_sniffer.cpp     # Main: BLE central, notify callback, loop
    ├── hid_peripheral.cpp  # BLE peripheral: HID GATT server, report sending
    ├── hid_peripheral.h
    ├── keymap.cpp          # Translation table, NVS persistence
    ├── keymap.h
    ├── web_config.cpp      # WiFi AP/STA, HTTP admin panel
    └── web_config.h
```

---

## Dependencies

| Library | Version |
|---------|---------|
| [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | `^1.4.3` |
| Preferences | bundled with ESP32 Arduino core |
| WebServer | bundled with ESP32 Arduino core |
| WiFi | bundled with ESP32 Arduino core |

Platform: `espressif32` (latest stable), framework: `arduino`.

---

## Technical notes

- **Dual BLE roles** — NimBLE-Arduino runs Central and Peripheral simultaneously on the single radio via time-slicing.
- **Connection intervals** — Both links are tuned to 7.5–15 ms (`minInterval=6, maxInterval=12` in BLE units). The Shield parameter update fires 3 s after connect to avoid disrupting Android's CCCD-write sequence.
- **Key-release pulse** — Keyboard-translated buttons (not consumer pass-throughs) get a forced 30 ms key-up to prevent the Shield from auto-repeating them on hold.
- **Consumer pass-throughs** — Released naturally when the TiVo remote sends its own key-up, preserving hold-to-repeat behaviour on nav keys.
- **NVS namespaces** — `tivo` (bond address), `keymap` (custom remaps), `cfg` (AP timeout, BLE power, WiFi credentials).
- **AP re-enable** — Hold the Boot button (GPIO 9) for 3 s to re-enable the WiFi AP if it has auto-timed-out.

---

## License

MIT — do whatever you like with it.
