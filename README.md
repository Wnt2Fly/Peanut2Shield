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

When a button is pressed on the TiVo remote, the firmware translates it (if needed) and forwards it to the Shield in real time.  Shield and TiVo bond addresses are stored in **NVS**; BLE keys persist across reboot when NimBLE bonding is enabled.

**Updating firmware:** [PlatformIO](#platformio-developers) for developers, or copy **[`usb-drive/`](#usb-reflash-kit-no-platformio)** to a USB stick and run **`flash-update.bat`** (Windows) or **`flash-update.sh`** (Linux) — Python + `esptool` only, no VS Code.

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

**TiVo remote feels sluggish or “buffered”** (press OK, long pause, then action): the ESP32 is often under-powered or starved on the data line, not a pairing bug. Try a **better USB cable** before changing firmware:

- **Data-capable** USB cable (many charge-only cables look fine but fail under load).
- **Short** run (6 in / 15 cm is ideal behind the TV).
- If you use **Shield USB-A → USB-C**, use a quality **USB-A to USB-C** adapter/cable rated for **3 A** where possible; thin phone-charge cables are a common cause.
- Compare with a **wall USB adapter** on the same board — if that feels snappy, swap the cable from the Shield.

---

## LED patterns

**Read the pattern, not just the colour** — the blink timing tells you what to do next.

| Colour | Pattern | Meaning |
|--------|---------|---------|
| Yellow | Steady | Boot — BLE stack initialising (a few seconds after reset) |
| Purple | Slow blink (500 ms on/off) | **Pair the Shield** — advertising, or Shield connected and finishing setup |
| Deep orange | Double-flash … pause … repeat | **Pair the TiVo remote** — Shield ready, scanning for TiVo; also BOOT 4 s warning |
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

If **Power**, **Volume**, **Input**, or **Mute** feel wrong over BLE (no response, wrong device, or fighting the TV), see **[Power & volume via TiVo remote IR](#power--volume-via-tivo-remote-ir-optional)** below.

---

## Power & volume via TiVo remote IR (optional)

Peanut2Shield forwards Power and Volume as BLE consumer keys. On some setups that is unreliable; however if you have an external AV system or soundbar, it may immediately work via CEC. If you have volume/power issues you can leave navigation and app keys on BLE through the bridge, and use the **TiVo remote’s own IR LED** for Power and Volume instead (programmed with a TV code).

The Stream 4K remote is not using the Shield’s IR blaster — it sends infrared from the remote body toward your TV or soundbar.

### Shield TV (CEC)

On the Shield, leave **main HDMI-CEC enabled**, but **turn off CEC for volume and power** so the Shield does not try to handle those buttons over CEC while the TiVo remote is using IR for them.

Exact menu names vary by Shield model and Android TV version; look under **Settings → Device Preferences → Display & Sound** (HDMI-CEC).

### TiVo remote — program IR codes (when CEC is not enough)

Use TiVo’s manual code entry so **Power**, **Volume**, **Mute**, **Input**, and **AV / amplifier** keys on the remote control your TV or soundbar over **IR**. Navigation and app keys still go to the Shield through Peanut2Shield over BLE.

**Code list (in this repo):** [`tivo_programming_codes.txt`](tivo_programming_codes.txt) — setup codes by brand plus step-by-step programming for each key group (TV power, input, volume/mute, audio amplifiers, etc.). Use this when CEC does not handle power, volume, input, or AV reliably.

**Also online:** [TiVoCommunity — disable automatic remote programming](https://www.tivocommunity.com/?threads/heres-how-to-disable-tivos-automatic-remote-programming.577390/) (same idea; the `.txt` file is easier to search offline on a USB stick copy of the project).

**Quick example (TV power — see the file for Input / Vol / Mute):**

1. Find your TV brand in `tivo_programming_codes.txt` and note the **first code** listed.
2. Hold **TiVo + TV Power** for **3 seconds** until the activity LED stays on.
3. Enter the code with the number keys; the LED blinks three times and turns off.
4. Point at the TV and press **TV Power** to test.
5. If it fails, repeat with the **next code** for that brand until Power (and, per the guide, Input / Vol / Mute) work.

This is independent of Peanut2Shield BLE pairing; program IR after OK, Home, and app keys work over the bridge.

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
4. LED stays **purple** while the Shield finishes setup, then switches to **deep orange double-flash** — pair the TiVo remote now

### 3 — Pair the TiVo remote  ← do this when you see deep orange double-flash

The bridge is already scanning. Put the **TiVo Stream 4K** remote in BLE pairing mode:

1. **Clear / reset the remote** (if it was paired elsewhere or acts stuck):
   - Hold **Power + TiVo** until the activity light turns **red**
   - Press **Volume Down** three times → light stays **solid red**
   - Press the **TiVo** button
2. **Enter pairing mode** — press **any button** on the remote (activity light should show it is searching)
3. Peanut2Shield connects automatically when it sees `TiVo Remote` advertising HID (`0x1812`)
4. Keep the link up **5 seconds** — LED stays deep orange briefly, then **3 green flashes** and **steady green**

### Re-pairing

| What to re-pair | Method | LED after |
|-----------------|--------|-----------|
| TiVo remote | Hold BOOT 5 s | Purple quick flash → deep orange double-flash; reset remote (Power+TiVo…) then any button for pairing |
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

Two ways to install or update firmware on the Waveshare ESP32-S3-Zero:

| Method | Who it's for | What you need |
|--------|----------------|---------------|
| **[USB reflash kit](#usb-reflash-kit-no-platformio)** | Family / another PC | `usb-drive/` on a USB stick, Python 3, `esptool` (`flash-update.bat` or `flash-update.sh`) |
| **PlatformIO** (below) | Developers editing the code | VS Code + PlatformIO, USB-C cable |

After any flash, press the board **RESET** button once. The ESP32-S3-Zero uses native USB-CDC, so upload does not auto-reboot the chip.

---

### USB reflash kit (no PlatformIO)

The **`usb-drive/`** folder is a portable kit — copy the whole directory to a USB stick for updates on another PC (e.g. a family member’s Windows or Linux machine).

**Contents:**

| Path | Purpose |
|------|---------|
| `START-HERE.txt` | Short instructions (start here) |
| `README-REFLASH.txt` / `README-REFLASH-LINUX.txt` | Full Windows / Linux guides |
| `flash-update.bat` / `flash-update.sh` | Normal update — writes app only at `0x10000` |
| `flash-full.bat` / `flash-full.sh` | Full chip image — use only if update fails or board is blank |
| `pack-usb-drive.bat` / `pack-usb-drive.sh` | **Maintainer:** build firmware and refresh `.bin` files in the kit |
| `firmware/firmware.bin` | App image used by `flash-update` (created by `pack-*`) |
| `firmware-full/` | Bootloader, partition table, `boot_app0`, app — used by `flash-full` |
| `vendor/boot_app0.bin` | Bundled Espressif boot stub so packing works without hunting PlatformIO paths |

**Prepare the stick (maintainer, from the project root):**

```bash
# Windows
usb-drive\pack-usb-drive.bat

# Linux
chmod +x usb-drive/pack-usb-drive.sh
./usb-drive/pack-usb-drive.sh
```

Then copy the entire **`usb-drive`** folder to the USB drive. Built `.bin` files are not always in git — run `pack-*` after each firmware change before copying to a stick.

**Flash on Windows:**

1. One-time: [Python 3](https://www.python.org/downloads/) (**Add to PATH**), then `pip install esptool`.
2. Plug the board in with a **data** USB-C cable.
3. Run **`flash-update.bat`** → enter COM port (Device Manager → Ports, e.g. `COM19`).
4. Press **RESET** once; plug back into TV power.

**Flash on Linux:**

1. One-time: `pip3 install --user esptool`.
2. Serial access: `sudo usermod -aG dialout $USER` then log out and back in.
3. From the `usb-drive` folder:
   ```bash
   chmod +x flash-update.sh flash-full.sh
   ./flash-update.sh
   ```
   Default port `/dev/ttyACM0` (Enter), or list devices with `ls /dev/ttyACM*`.

**Pairing:** `flash-update` usually **keeps** Shield and TiVo Bluetooth bonds. **`flash-full`** rewrites the whole flash — expect to **re-pair** both devices (see [First-time pairing](#first-time-pairing)).

---

### PlatformIO (developers)

**Requirements:** [PlatformIO](https://platformio.org/) CLI or VS Code extension, USB-C cable.

**Build:**

```bash
pio run
```

**Flash:**

```bash
pio run --target upload
```

**Monitor** (115200 baud — useful while pairing):

```bash
pio device monitor -p COM<N> -b 115200
```

---

## Project structure

```
├── LICENSE                     # MIT license
├── tivo_programming_codes.txt  # TiVo IR codes (power, vol, input, AV) if CEC fails
├── usb-drive/                  # USB stick reflash kit (see Building & flashing)
│   ├── START-HERE.txt
│   ├── flash-update.bat / .sh
│   ├── flash-full.bat / .sh
│   ├── pack-usb-drive.bat / .sh
│   ├── firmware/               # firmware.bin (from pack-*)
│   ├── firmware-full/          # full flash set (from pack-*)
│   └── vendor/boot_app0.bin
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
- **NVS namespaces** — `tivo` (address + `trusted` after 5 s link), `shield` (address after CCCD), `keymap` (custom remaps — storage only; no runtime UI yet). NimBLE bond keys use `CONFIG_BT_NIMBLE_NVS_PERSIST` in `platformio.ini`.
- **LED** — Non-blocking state machine driven by `ledTick()` in `loop()`. Priority: Activity (white) > base pattern. Purple slow blink = Shield pairing; deep orange double-flash = TiVo pairing; green = ready. Global brightness controlled by `CFG_LED_BRIGHTNESS` in `config.h`.
- **All constants** — Every timing value, pin number, BLE parameter, and default keymap entry lives in `src/config.h`. No magic numbers anywhere else.

---

## License

[MIT](LICENSE) — do whatever you like with it.

---

## Support

If this project saved you a broken remote or a pile of HDMI switches, consider **[buying me a coffee](https://www.buymeacoffee.com/wnt2fly)** — same link as my [F1-Info-display-Enhanced](https://github.com/Wnt2Fly/F1-Info-display-Enhanced) project.

