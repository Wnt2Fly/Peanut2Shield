# Peanut2Shield — TiVo Remote BLE HID Translator

An ESP32-S3 firmware that bridges a **TiVo Stream 4K remote** to an **Nvidia Shield TV** over Bluetooth LE — no WiFi, no app, no cloud.

<a href="https://www.buymeacoffee.com/wnt2fly" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="35" width="auto"></a>

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Zero-Waveshare-blue) ![BLE](https://img.shields.io/badge/BLE-HID-blueviolet) ![NVIDIA Shield](https://img.shields.io/badge/NVIDIA-Shield%20TV-76B900) ![TiVo](https://img.shields.io/badge/TiVo-Stream%204K%20Remote-orange) ![PlatformIO](https://img.shields.io/badge/PlatformIO-espressif32-FBC02D)

**Topics:** `esp32` · `esp32-s3` · `ble` · `bluetooth-low-energy` · `hid` · `nimble` · `nvidia-shield` · `android-tv` · `tivo` · `tivo-remote` · `platformio` · `waveshare`

---

## What it does

```
TiVo Remote  ──BLE HID──►  ESP32-S3  ──BLE HID──►  Nvidia Shield TV
              (Central)   Translator   (Peripheral)
```

The ESP32-S3 simultaneously acts as:

- **BLE Central** — connects to the TiVo remote as a HID host and receives button reports
- **BLE Peripheral** — advertises as a HID keyboard + consumer control device to the Shield

When a button is pressed on the TiVo remote, the firmware translates it (if needed) and forwards it to the Shield in real time.  BLE bonds are stored in NVS and survive reboots.

---

## Hardware

| Part | Detail |
|------|--------|
| MCU | [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm) (4 MB flash, 2 MB PSRAM) |
| LED | WS2812 RGB on GPIO 21 — colour-coded by connection state (see LED patterns below) |
| Boot button | GPIO 0 — hold to manage bonds (see below) |
| Power | USB-C — must stay powered continuously (see [Power](#power) below) |

---

## Power

The bridge must remain powered for BLE to work — it has no battery.

- Plug into a **USB port that is always on** (wall adapter, powered hub, or TV USB port that stays live in standby).
- If you power it from the **Shield's USB port**, check that those ports are **not disabled when the Shield sleeps or powers down**. Some Shield models turn off rear USB ports in deep sleep; if power drops, the bridge reboots and you lose the active BLE session until it reconnects.
- A dedicated wall-powered USB adapter behind the TV is the most reliable option.

---

## LED patterns

**Read the pattern, not just the colour** — the blink timing tells you what to do next.

| Colour | Pattern | Meaning |
|--------|---------|---------|
| Yellow | Steady | Boot — BLE stack initialising (a few seconds after reset) |
| Purple | Slow blink (500 ms on/off) | **Pair the Shield now** — advertising, no Shield bonded yet |
| Orange | Double-flash … pause … repeat | Shield paired — put TiVo remote in pairing mode; also BOOT 4 s warning |
| Green | 3 quick flashes (once), then **steady on** | Both devices ready — stays green while paired |
| White | Single 80 ms flash | Button press forwarded to Shield |
| Yellow | Fast blink (250 ms on/off) | BOOT held — counting toward 4/5/8/10 s |
| Purple | 3 quick flashes (once) | TiVo bond cleared (BOOT 5 s) |
| Purple | Double-flash … pause … repeat | Shield bond cleared (BOOT 8 s) |
| Red | 3 quick flashes (once) | Factory reset starting (BOOT 10 s) |
| Green | 3 quick flashes (once) | Factory reset confirm → then slow blink |

> **Note:** Earlier firmware used the wrong WS2812 channel order (`NEO_GRB`), which made the purple slow-blink state look **cyan** on the Waveshare ESP32-S3-Zero. Reflash if colours still look swapped (e.g. cyan when you expect purple).

---

## Boot button actions

Hold the **BOOT** button (GPIO 0) without releasing; actions fire automatically at each threshold. While you hold (before any threshold), the LED **blinks yellow quickly** so you know the timer is running.

| Hold time | Action | LED after threshold |
|-----------|--------|---------------------|
| **(any hold start)** | — | Fast **yellow** blink while counting |
| **4 s** | Warning only — keep holding | **Orange** double-flash |
| **5 s** | Forget TiVo bond → restart TiVo scan | **Purple** 3 quick flashes; resumes yellow if still held |
| **8 s** | Forget Shield bond → restart advertising | **Purple** double-flash |
| **10 s** | Factory reset — forget both bonds, erase keymap NVS | **Red** 3 quick flashes → **green** 3 quick flashes → slow blink |

---

## Default key translation table

| TiVo button | Usage code | Forwarded as |
|-------------|------------|--------------|
| TiVo | `0x003D` | Keyboard F4 |
| Live TV | `0x003E` | Keyboard F7 |
| Guide | `0x008D` | Keyboard F8 |
| Info | `0x0209` | Keyboard F9 |
| Skip | `0xCE00` | Keyboard F10 |
| Netflix | `0x01C8` | Keyboard F11 |
| Home | `0x0223` | Keyboard Home |
| Back | `0x0224` | Keyboard ESC |
| OK / Select | `0x0041` | Consumer pass-through |
| Power | `0x0030` | Consumer pass-through (forced 30 ms release pulse) |
| Navigation (▲▼◀▶) | `0x0042`–`0x0045` | Consumer pass-through; relaxed dedup (see below) |
| All other buttons | — | Consumer pass-through |

Keyboard-translated buttons get a forced **30 ms key-up pulse** so the Shield doesn't auto-repeat them.  Navigation keys skip the bounce guard and release immediately on all-zero idle reports from the TiVo's dual consumer characteristics, so directional repeat works naturally.  Identical-report hold dedup still applies to all keys.

To launch apps or change what a button does on the Shield, use **[Button Mapper](#custom-button-mapping-shield-side)** (recommended).  To change what the bridge sends before it reaches the Shield, edit `CFG_DEFAULT_KEYMAP` in `src/config.h` and reflash.

---

## Custom button mapping (Shield side)

Peanut2Shield turns several TiVo buttons into **keyboard F-keys** (see table above).  On the Shield, map those keys to any app with **[Button Mapper](https://play.google.com/store/apps/details?id=flar2.homebutton)** by flar2 (free, Play Store) — no root, no firmware reflash.

### Setup Button Mapper (one time)

1. On the Shield: install **Button Mapper** from the Play Store.
2. Open it and allow **Accessibility** when prompted: **Settings → Device Preferences → Accessibility → Button Mapper → ON**.
3. In Button Mapper, choose **Add buttons**.
4. Press the TiVo button you want to remap (via Peanut2Shield).  It should appear as a key (e.g. **F7** for Live TV).
5. Turn **Customize** on, set **Single tap** (or **Long press**, if you prefer) to **Applications**, and pick the app.
6. If the key is not detected or the old action still fires, open **Troubleshooting** in Button Mapper and try **Alternate button handling**.

### Example: Live TV → YouTube TV

The bridge sends **F7** when you press **Live TV** on the TiVo remote.

1. **Add buttons** → press **Live TV** on the TiVo remote → select **F7**.
2. **Customize → Single tap → Applications → YouTube TV**.
3. Press **Live TV** again — YouTube TV should open.

Same idea for other mapped buttons:

| TiVo button | Key sent to Shield | Example remap |
|-------------|-------------------|---------------|
| TiVo | F4 | Your DVR app |
| Live TV | F7 | YouTube TV |
| Guide | F8 | Plex |
| Info | F9 | Any app |
| Skip | F10 | Any app |
| Netflix | F11 | Keep Netflix or map to another app |

Consumer pass-through buttons (OK, nav, volume, etc.) are not F-keys — remap them in Button Mapper the same way if the app detects the consumer key, or change the firmware default in `config.h`.

### Firmware-level remapping (advanced)

Edit `CFG_DEFAULT_KEYMAP` in `src/config.h` if you need the bridge to emit a different key or consumer code, then reflash.  NVS custom remap storage exists in code but has no runtime UI yet; factory reset clears stored custom entries.

---

## First-time pairing

After reset, the normal sequence is: **steady yellow** (brief, while BLE starts) → **slow blink** (500 ms on / 500 ms off) → **double-flash** → **3 quick green flashes** → **steady green**.

If you only see a **slow blink** and never noticed yellow, that is fine — slow blink means you are on step 2.

### 1 — Power on

Press **RESET** (or power on). The LED may show **steady yellow** briefly while the BLE stack starts. When it settles into a **slow blink** (lit half a second, dark half a second), the bridge is advertising and waiting for the Shield.

### 2 — Pair the Shield  ← do this when you see slow blink

1. On the Shield: **Settings → Remote & Accessories → Add Accessory**
2. Select **Peanut2Shield** — it pairs automatically (no PIN)
3. Keep **slow blink** while the Shield finishes setup (CCCD negotiation) — do not pair the TiVo yet
4. When setup completes, LED switches to **orange double-flash** — Shield is ready, now waiting for the TiVo remote

### 3 — Pair the TiVo remote  ← do this when you see double-flash

1. On the TiVo remote hold **TiVo + Back** until the remote's light flashes to enter pairing mode
2. The device is already scanning — it connects automatically
3. LED plays **3 green flashes** then stays **steady green** — both devices are ready and translating

### Re-pairing

| What to re-pair | Method | LED after |
|-----------------|--------|-----------|
| TiVo remote | Hold BOOT 5 s | Purple quick flash → orange double-flash (waiting for TiVo) |
| Shield | Hold BOOT 8 s | Purple double-flash → slow blink (advertising for Shield) |
| Both | Hold BOOT 10 s | Red quick flash → green quick flash → slow blink |

---

## 3D-printed case

A snap-together enclosure designed specifically for the **Waveshare ESP32-S3-Zero** is included in the `case/` folder.

### Parts

| File | Description | Suggested material |
|------|-------------|-------------------|
| `case/base.stl` | Bottom tray — holds the board, USB-C side opening | Any PLA/PETG |
| `case/top.stl` | Snap-on lid — engraved button labels, LED hole | Any PLA/PETG |
| `case/led.stl` | Clear LED light-pipe insert (captured in lid) | Clear/transparent PETG or resin |

### Printing notes

- Print `base.stl` and `top.stl` at **0.2 mm layer height**, 3 perimeters, 15% infill — no supports needed
- Print `led.stl` in **clear filament** at 0.1 mm layers for best light transmission; orient flat-side down
- The lid snaps onto the tray — no hardware required
- The light-pipe press-fits into the lid from below and is retained by a small flange

### Modifying the design

The source file `case/waveshare esp32-s3-zero_case.scad` is a parametric [OpenSCAD](https://openscad.org/) model (v32).  Open it in OpenSCAD and use **Render → Export as STL** with the export variable set to the part you want (`top`, `base`, or `led_insert`).

---

## Building & flashing

### Requirements

- [PlatformIO](https://platformio.org/) CLI or VS Code extension
- Waveshare ESP32-S3-Zero connected via USB-C

### Build

```bash
pio run
```

### Flash

```bash
pio run --target upload
```

After the write reaches 100 %, press the **RESET** button on the board.  
The ESP32-S3-Zero uses native USB-CDC (no UART bridge chip), so the auto-reset after upload is not supported — a manual reset is required.

### Monitor

Serial debug output at 115200 baud is useful during pairing and button testing:

```bash
pio device monitor -p COM<N> -b 115200
```

---

## Project structure

```
├── LICENSE                     # MIT license
├── platformio.ini              # Board, platform, library dependencies
├── case/
│   ├── base.stl                # Bottom tray (3D print)
│   ├── top.stl                 # Snap-on lid (3D print)
│   ├── led.stl                 # Clear LED light-pipe insert (3D print)
│   └── waveshare esp32-s3-zero_case.scad   # Parametric OpenSCAD source
└── src/
    ├── config.h                # All compile-time constants (edit here to customise)
    ├── Peanut2Shield.cpp       # Main: BLE central, notify callback, LED engine, button handler
    ├── hid_peripheral.cpp      # BLE peripheral: HID GATT server, report sending
    ├── hid_peripheral.h
    ├── keymap.cpp              # Translation table, NVS persistence
    └── keymap.h
```

---

## Dependencies

| Library | Version |
|---------|---------|
| [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | `^1.4.3` |
| [adafruit/Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) | `^1.12.0` |
| Preferences | bundled with ESP32 Arduino core |

Platform: `espressif32`, framework: `arduino`, board: `esp32-s3-devkitc-1` with `board_build.flash_size = 4MB`.

---

## Technical notes

- **Dual BLE roles** — NimBLE-Arduino runs Central and Peripheral simultaneously on the single radio via time-slicing.
- **Connection intervals** — Both links target 7.5–15 ms (`minInterval=6, maxInterval=12` in BLE units). The Shield parameter update fires 1 s after CCCD write to avoid disrupting Android's service-discovery sequence.
- **Key-release pulse** — Keyboard-translated buttons get a forced 30 ms key-up (`CFG_KB_PULSE_MS`) so the Shield doesn't auto-repeat them on hold.
- **Bounce guard** — `CFG_BOUNCE_GUARD_ACTION_MS` suppresses the same usage code arriving too soon after key-up (currently **0 ms**, disabled).  Increase if action buttons double-fire.  Nav keys always skip it.
- **Nav key fast-path** — Navigation keys (0x0042–0x0045) skip the bounce guard and release immediately on all-zero idle reports instead of waiting for the 50 ms all-zero guard (`CFG_ALL_ZERO_GUARD_MS`).  Identical-report hold dedup still applies to all keys.
- **Power key pulse** — The Power key (0x0030) also gets a forced 30 ms release rather than relying on the TiVo's key-up timing.
- **NVS namespaces** — `tivo` (bond address), `keymap` (custom remaps — storage only; no runtime UI yet).
- **LED** — Non-blocking state machine driven by `ledTick()` in `loop()`. Priority: Activity (white) > base pattern. Each state has a distinct colour: yellow=boot, purple=advertising, orange=waiting for TiVo, green=ready (steady on), red=factory reset. Global brightness controlled by `CFG_LED_BRIGHTNESS` in `config.h`.
- **All constants** — Every timing value, pin number, BLE parameter, and default keymap entry lives in `src/config.h`. No magic numbers anywhere else.

---

## License

[MIT](LICENSE) — do whatever you like with it.

---

## Support

If this project saved you a broken remote or a pile of HDMI switches, consider **[buying me a coffee](https://www.buymeacoffee.com/wnt2fly)** — same link as my [F1-Info-display-Enhanced](https://github.com/Wnt2Fly/F1-Info-display-Enhanced) project.

