# Peanut2Shield — TiVo Remote BLE HID Translator

An ESP32-S3 firmware that bridges a **TiVo Stream 4K remote** to an **Nvidia Shield TV** over Bluetooth LE — no WiFi, no app, no cloud.

---

## What it does

```
TiVo Remote  ──BLE HID──►  ESP32-S3  ──BLE HID──►  Nvidia Shield TV
              (Central)   Translator   (Peripheral)
```

The ESP32-S3 simultaneously acts as:

- **BLE Central** — connects to the TiVo remote as a HID host and receives button reports
- **BLE Peripheral** — advertises as a HID keyboard + consumer control device to the Shield

When a button is pressed on the TiVo remote, the firmware translates it (if needed) and forwards it to the Shield in real time.  All bonds and custom keymaps are stored in NVS and survive reboots.

---

## Hardware

| Part | Detail |
|------|--------|
| MCU | [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm) (4 MB flash, 2 MB PSRAM) |
| LED | WS2812 RGB on GPIO 21 — colour-coded by connection state (see LED patterns below) |
| Boot button | GPIO 0 — hold to manage bonds (see below) |

---

## LED patterns

| Colour | Pattern | Meaning |
|--------|---------|---------|
| Yellow | Steady | Boot — on while BLE stack initialises (~0.5 s) |
| Purple | Slow blink (500 ms on/off) | Advertising — waiting for Shield to pair |
| Orange | Double-flash … pause … repeat | Shield paired, waiting for TiVo remote to bond |
| Green | 3 quick flashes (once) | Both devices ready — LED then goes off |
| White | Single 80 ms flash | Button press forwarded to Shield |
| Red | Rapid 100 ms flicker | Factory reset hold in progress |

---

## Boot button actions

Hold the **BOOT** button (GPIO 0) without releasing; actions fire automatically at each threshold:

| Hold time | Action |
|-----------|--------|
| **3 s** | Forget TiVo bond → LED DoubleFlash → restart TiVo scan |
| **6 s** | Forget Shield bond → LED SlowBlink → restart advertising |
| **10 s** | Factory reset — forget both bonds, erase keymap NVS → LED rapid flicker → 3-flash → SlowBlink |

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
| Navigation (▲▼◀▶) | `0x0042`–`0x0045` | Consumer pass-through, no repeat guard |
| All other buttons | — | Consumer pass-through |

Keyboard-translated buttons get a forced **30 ms key-up pulse** so the Shield doesn't auto-repeat them.  Navigation keys bypass the bounce guard entirely, letting the TiVo's natural repeat flow through.

Custom remaps are stored in NVS and override the table above. Edit `CFG_DEFAULT_KEYMAP` in `src/config.h` to change the firmware defaults.

---

## First-time pairing

### 1 — Power on

LED turns **yellow** (steady) while the BLE stack boots (~0.5 s), then immediately switches to **purple slow-blink** — the device is advertising and waiting for the Shield.

### 2 — Pair the Shield

1. On the Shield: **Settings → Remote & Accessories → Add Accessory**
2. Select **TiVo-Bridge** — it pairs automatically (no PIN)
3. LED switches to **orange double-flash** — Shield is bonded, now waiting for the TiVo remote

### 3 — Pair the TiVo remote

1. On the TiVo remote hold **TiVo + Back** until the remote's light flashes to enter pairing mode
2. The device is already scanning — it connects automatically
3. LED plays **3 green flashes** then goes off — both devices are ready and translating

### Re-pairing

| What to re-pair | Method | LED after |
|-----------------|--------|-----------|
| TiVo remote | Hold BOOT 3 s | Orange double-flash (scanning for TiVo) |
| Shield | Hold BOOT 6 s | Purple slow-blink (advertising for Shield) |
| Both | Hold BOOT 10 s | Red rapid flicker → 3 green flashes → purple slow-blink |

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

```bash
pio device monitor -p COM<N> -b 115200
```

---

## Project structure

```
├── platformio.ini          # Board, platform, library dependencies
└── src/
    ├── config.h            # All compile-time constants (edit here to customise)
    ├── Peanut2Shield.cpp   # Main: BLE central, notify callback, LED engine, button handler
    ├── hid_peripheral.cpp  # BLE peripheral: HID GATT server, report sending
    ├── hid_peripheral.h
    ├── keymap.cpp          # Translation table, NVS persistence
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
- **Nav key fast-path** — Navigation keys (0x0042–0x0045) bypass the 300 ms bounce guard and the all-zero guard entirely, so the TiVo's natural repeat rate flows through unthrottled.
- **Power key pulse** — The Power key (0x0030) also gets a forced 30 ms release rather than relying on the TiVo's key-up timing.
- **NVS namespaces** — `tivo` (bond address), `keymap` (custom remaps).
- **LED** — Non-blocking state machine driven by `ledTick()` in `loop()`. Priority: Activity (white) > base pattern. Each state has a distinct colour: yellow=boot, purple=advertising, orange=waiting for TiVo, green=ready, red=factory reset. Global brightness controlled by `CFG_LED_BRIGHTNESS` in `config.h`.
- **All constants** — Every timing value, pin number, BLE parameter, and default keymap entry lives in `src/config.h`. No magic numbers anywhere else.

---

## License

MIT — do whatever you like with it.
