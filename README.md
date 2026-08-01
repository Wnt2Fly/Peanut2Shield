# Peanut2Shield — TiVo Remote BLE HID Translator

**Firmware v1.09**

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

When a button is pressed on the TiVo remote, the firmware translates it (if needed) and forwards it to the Shield in real time.  Shield and TiVo bond addresses are stored in **NVS**; BLE keys persist across reboot when NimBLE bonding is enabled. After a normal reset or power cycle, the bridge **reconnects to both devices automatically** — Shield first (while advertising), then TiVo — and returns to **steady green** when both links are up.

**Updating firmware:** [PlatformIO](#platformio-developers) for developers, or copy **[`usb-drive/`](#usb-update-kit-no-platformio)** to a USB stick — **`UPDATE.bat`** (Windows) or **`UPDATE.sh`** (Linux).

---

## Hardware

| Part | Detail |
|------|--------|
| MCU | [Waveshare ESP32-S3-Zero](https://www.waveshare.com/esp32-s3-zero.htm) (chip **ESP32-S3FH4R2** — 4 MB flash, **2 MB PSRAM required**) |
| LED | WS2812 RGB on GPIO 21 — colour-coded by connection state (see LED patterns below) |
| Boot button | GPIO 0 — hold to manage bonds (see below) |
| Power | USB-C — wall USB adapter preferred (see [Power](#power); avoid Shield USB data cable) |

---

## Power

The bridge must remain powered for BLE to work — it has no battery.

**Recommended:** a dedicated **wall USB adapter** (or always-on powered hub / TV USB that stays live in standby) behind the TV. Power-only sources are the most reliable.

### Do not power from the Shield USB port (recommended)

Earlier docs suggested the Shield’s USB port as an option. **Avoid that for day-to-day use.**

The Waveshare ESP32-S3-Zero exposes **USB Serial/JTAG** on its USB-C jack. The Nvidia Shield is a USB **host**: it often **enumerates** that serial device but never opens it. Firmware debug writes can then **stall**, the LED freezes (commonly **solid purple**), and BLE stops making progress — even though pairing was fine for days on wall power.

The Shield also has no practical setting to “ignore COM / USB serial gadgets” for this device.

| Power source | Notes |
|--------------|--------|
| **Wall USB adapter** | Best — power only; no host grabbing USB serial |
| Powered hub / always-on TV USB | Good if it stays powered in standby |
| **Shield USB** | Not recommended — sleep/cut power *and* USB-serial hang risk |
| PC USB (no serial monitor) | Same hang risk as Shield USB; opening a COM monitor can unblock it |

**If you must use Shield USB anyway:** use a **charge-only** (power-only) USB cable or adapter that does **not** connect the data lines (D+/D−). That stops the Shield from talking USB serial to the board while still supplying 5 V. A normal data cable to the Shield is what triggers the hang.

**TiVo remote feels sluggish or “buffered”** (press OK, long pause, then action): the ESP32 is often under-powered or on a weak cable, not a pairing bug:

- Prefer a short, decent **wall-adapter** cable.
- Thin phone-charge cables and long runs are a common cause of lag.
- If you previously used Shield USB-A → USB-C for power, move to a wall adapter first before chasing firmware.

---

## LED patterns

**Read the pattern, not just the colour** — the blink timing tells you what to do next.

| Colour | Pattern | Meaning |
|--------|---------|---------|
| Yellow | Steady | Boot — BLE stack initialising (~1–2 s after reset). |
| Purple | Slow blink (500 ms on/off) | **Pair / wait for Shield** — advertising, or Shield connected and finishing setup |
| Purple | **Solid** (not blinking) | **Stuck** — often USB host hang (Shield USB or PC USB with no monitor). Use wall power; see [Troubleshooting](#troubleshooting). |
| Deep orange | Double-flash … pause … repeat | **Pair the TiVo remote** — Shield ready, scanning for TiVo; also BOOT 4 s warning |
| Green | 3 quick flashes (once), then **steady on** | Both devices ready — stays green while paired |
| White | Single 80 ms flash | Button press forwarded to Shield |
| Yellow | Fast blink (250 ms on/off) | BOOT held — counting toward 4/5/8/10 s |
| Purple | 3 quick flashes (once) | TiVo bond cleared (BOOT 5 s) |
| Purple | Double-flash … pause … repeat | Shield bond cleared (BOOT 8 s) |
| Red | 3 quick flashes (once) | Factory reset starting (BOOT 10 s) |
| Red | Slow blink (400 ms on/off) | **Wrong hardware** — no PSRAM detected; needs genuine Waveshare FH4R2 board |
| Green | 3 quick flashes (once) | Factory reset confirm → then slow blink |

> **Stuck on yellow?** Steady yellow for more than ~5 s usually means a **crash reboot loop** (BLE failed to start), not pairing mode.  
> **Stuck on solid purple?** Firmware hung after start (often Shield/PC USB serial) — use a **wall adapter**, not Shield USB. See [Troubleshooting](#troubleshooting).

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
| Power | `0x0030` | **Ignored over BLE by default** (`CFG_IGNORE_TIVO_POWER_BLE=1`) — use TiVo IR Power; set to `0` in `config.h` to forward to Shield |
| Vol+ / Vol− / Mute | `0x00E9` / `0x00EA` / `0x00E2` | **Ignored over BLE by default** (`CFG_IGNORE_TIVO_VOLUME_BLE=1`) — use TiVo IR volume; set to `0` in `config.h` to forward to Shield |
| Navigation (▲▼◀▶) | `0x0042`–`0x0045` | Consumer pass-through; relaxed dedup (see below) |
| All other buttons | — | Consumer pass-through |

Keyboard-translated buttons get a forced **30 ms key-up pulse** so the Shield doesn't auto-repeat them.  Navigation keys skip the bounce guard and release immediately on all-zero idle reports from the TiVo's dual consumer characteristics, so directional repeat works naturally.  Identical-report hold dedup still applies to all keys.

To launch apps or change what a button does on the Shield, use **[Button Mapper](#custom-button-mapping-shield-side)** (recommended).  To change what the bridge sends before it reaches the Shield, edit `CFG_DEFAULT_KEYMAP` in `src/config.h` and reflash.

**Power**, **Volume**, and **Mute** are ignored over BLE by default so they do not fight the TiVo remote’s IR (or CEC). See **[Power & volume via TiVo remote IR](#power--volume-via-tivo-remote-ir)** below. For **Input** or other IR-only keys, program IR on the remote the same way.

---

## Power & volume via TiVo remote IR

**Power, Vol+, Vol−, and Mute over BLE are ignored by default** (`CFG_IGNORE_TIVO_POWER_BLE=1` and `CFG_IGNORE_TIVO_VOLUME_BLE=1` in `config.h`). The TiVo remote still emits those keys on BLE, but Peanut2Shield does **not** forward them to the Shield — that was sleeping/waking the Shield (Power) or double-adjusting volume with IR/CEC. Program the remote’s **IR Power** and **IR Volume/Mute** for the TV or amp; navigation and app keys still go over BLE.

| Flag in `config.h` | Default | Effect when `1` |
|--------------------|---------|-----------------|
| `CFG_IGNORE_TIVO_POWER_BLE` | `1` | Do not forward Power (`0x0030`) over BLE |
| `CFG_IGNORE_TIVO_VOLUME_BLE` | `1` | Do not forward Vol+ / Vol− / Mute (`0x00E9` / `0x00EA` / `0x00E2`) over BLE |

Set either flag to `0` and reflash only if you want that key group over BLE again.

The Stream 4K remote is not using the Shield’s IR blaster — it sends infrared from the remote body toward your TV or soundbar.

### Shield TV (CEC)

On the Shield, leave **main HDMI-CEC enabled**, but **turn off CEC for volume and power** so the Shield does not fight IR for those keys.

Exact menu names vary by Shield model and Android TV version; look under **Settings → Device Preferences → Display & Sound** (HDMI-CEC).

### TiVo remote — program IR codes

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

Consumer pass-through buttons (OK, nav, etc.) are not F-keys — remap them in Button Mapper the same way if the app detects the consumer key, or change the firmware default in `config.h`. Volume/Mute/Power are not forwarded over BLE by default (IR); see above.

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

### After reboot

If both devices were paired before, you do **not** need to open Shield settings or put the TiVo remote in pairing mode again.

Power from a **wall USB adapter** (not Shield USB with a data cable). See [Power](#power).

1. Power on or press **RESET** — brief **yellow**, then **purple slow blink** while the Shield reconnects (up to ~8 s if the Shield was asleep).
2. TiVo reconnects automatically once the Shield window finishes (or sooner if Shield is already linked).
3. **Steady green** = both ready; white flash on button press = keys reaching the Shield.

If the LED is **solid purple** (not blinking), the board is hung on USB serial — move to wall power or a charge-only cable.

If you see **white flashes** but the Shield does not respond, the TiVo link is up but the Shield is not — wait a few seconds, wake the Shield, or hold **BOOT 8 s** to re-pair the Shield side only.

**Shield powered off while TiVo stays paired:** the bridge keeps the TiVo link when possible and uses **slow advertising** so re-advertising for the Shield does not starve the remote. When the Shield comes back, it should reconnect without forcing a TiVo re-pair. If the remote went to sleep, press any button once to wake it.

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
| **[USB update kit](#usb-update-kit-no-platformio)** | Family / another PC | `usb-drive/` — **`UPDATE.bat`** (Windows, no Python) |
| **PlatformIO** (below) | Developers editing the code | VS Code + PlatformIO, USB-C cable |

After any flash, press the board **RESET** button once. The ESP32-S3-Zero uses native USB-CDC, so upload does not auto-reboot the chip.

---

### USB update kit (no PlatformIO)

Copy the entire **`usb-drive/`** folder to a USB stick. On **Windows, no Python** is required (`tools/win/espflash.exe` is bundled).

**Layout:**

```
usb-drive/
├── UPDATE.bat              ← double-click (Windows)
├── UPDATE.sh               ← Linux menu
├── START-HERE.txt          ← user guide (read this)
├── VERSION.txt
├── docs/
│   ├── windows.txt
│   └── linux.txt
├── firmware/
│   ├── update/firmware.bin   — app-only (normal update)
│   └── full/                 — bootloader + partitions + app
├── tools/
│   ├── win/                  — espflash.exe + flash scripts
│   └── linux/                — flash scripts (needs pip esptool)
└── pack/                     — maintainer: build + bundle (ignore on stick)
    ├── pack.bat / pack.sh
    └── vendor/boot_app0.bin
```

**Build the kit (maintainer, from project root):**

```bat
pack-usb-drive.bat
rem or:  usb-drive\pack\pack.bat
```

```bash
./pack-usb-drive.sh
rem or:  ./usb-drive/pack/pack.sh
```

Then copy **`usb-drive/`** to the stick. Re-run after each firmware change.

**End user (Windows):** double-click **`UPDATE.bat`** → see **`START-HERE.txt`** if you want steps first.

**End user (Linux):** `./UPDATE.sh` → see **`docs/linux.txt`** (Python + esptool one-time).

**Pairing:** option 1 usually keeps bonds. Option 2 (full fix) erases the chip — re-pair Shield and TiVo.

**Yellow LED stuck:** use **UPDATE.bat → option 2** (full erase + flash).

---

### PlatformIO (developers)

**Requirements:** [PlatformIO](https://platformio.org/) CLI or VS Code extension, USB-C **data** cable.

**Build:**

```bash
pio run
```

**Flash** (close any serial monitor first — the COM port can only be used by one program):

```bash
pio run --target upload
```

**Recover a crash-looping board** (full erase + upload — same as UPDATE.bat option 2):

```bash
# Windows — double-click or:
flash-recover.bat COM20

# Or manually:
pio run -t erase --upload-port COM20
pio run -t upload --upload-port COM20
```

**Monitor** (115200 baud — useful while pairing):

```bash
pio device monitor -p COM20 -b 115200
```

Good boot on serial:

```
=== TiVo BLE HID Translator v1.02 ===
[BOOT] flash=4096 KB  PSRAM=2048 KB  heap=...
[HID] Peripheral ready — advertising as 'Peanut2Shield'.
```

On PC USB power, expect brief **yellow**, then **purple slow blink** if nothing is paired yet. Opening the serial monitor can change behaviour: with no reader, USB serial writes may **block** and freeze the LED (solid purple/yellow). Prefer wall power for normal TV use.

---

## Troubleshooting

### LED stuck on yellow or solid purple

| Symptom | Likely cause | Fix |
|---------|----------------|-----|
| Yellow **~1–2 s**, then purple **blink** | Normal boot | Pair Shield if new; ignore if already green behind TV |
| Yellow **forever** or keeps restarting | BLE crash loop (`ESP_ERR_NO_MEM` on serial) | **`UPDATE.bat` → option 2** or **`flash-recover.bat COM<N>`** |
| **Solid purple** (not blinking) after power cycle | USB host enumerated serial but nothing reads it (Shield USB / PC without monitor) | Power from a **wall USB adapter**; avoid Shield USB. Optional: charge-only cable if you must use Shield power |
| **Fast** yellow blink | BOOT button held or stuck | Release BOOT; check case isn’t pressing the button |
| **Red** slow blink | No PSRAM on chip | Wrong board — need **Waveshare ESP32-S3-Zero (FH4R2)** with 2 MB PSRAM |

### Serial shows `ESP_ERR_NO_MEM` or `Config struct mismatch`

Usually **stale NVS** after a **USB option 1** (app-only) update, or PSRAM not enabled. The app partition updates but old Bluetooth settings stay behind and v1.02 could crash-loop.

Fix:

1. Close the serial monitor (Ctrl+C).
2. **UPDATE.bat → option 2** (full erase + flash), or `flash-recover.bat COM<N>` from a dev PC.
3. Press **RESET** once; expect **purple slow blink** within a few seconds.

**v1.03+** clears incompatible NVS on first boot after a bad update (re-pair once).

Normal update (option 1) does **not** clear NVS — do **not** use option 1 on a yellow/crash-looping board.

### Upload / flash errors

| Error | Fix |
|-------|-----|
| `Cannot configure port` / port missing | Serial monitor still open — close it first |
| `No serial data received` | Unplug/replug USB, press RESET, try another cable/port |
| Upload OK but still yellow | Run **erase** then upload again |

### White flashes but Shield ignores buttons

TiVo is connected but Shield is not. Wait ~10 s after reboot (v1.02 reconnect window), wake the Shield, or hold **BOOT 8 s** to re-pair Shield only.

---

## Project structure

```
├── LICENSE                     # MIT license
├── flash-recover.bat           # Windows: erase + upload (recover crash loop)
├── sdkconfig.defaults          # PSRAM / BLE memory settings for ESP32-S3-Zero
├── tivo_programming_codes.txt  # TiVo IR codes (power, vol, input, AV) if CEC fails
├── pack-usb-drive.bat          # wrapper → usb-drive/pack/pack.bat
├── usb-drive/                  # USB stick update kit
│   ├── UPDATE.bat / UPDATE.sh
│   ├── START-HERE.txt
│   ├── VERSION.txt
│   ├── docs/windows.txt / linux.txt
│   ├── firmware/update/ / full/
│   ├── tools/win/ / linux/
│   └── pack/pack.bat / pack.sh
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

Platform: `espressif32`, framework: `arduino`, board: `esp32-s3-devkitc-1` with `board_build.flash_size = 4MB`, `board_build.arduino.memory_type = qio_qspi`, and `sdkconfig.defaults` for quad PSRAM. Firmware version: `CFG_FIRMWARE_VERSION` in `src/config.h`.

---

## Technical notes

- **PSRAM required** — Dual BLE (central + peripheral) needs the **2 MB PSRAM** on the ESP32-S3FH4R2. At boot the firmware logs `[BOOT] PSRAM=… KB`; values under ~512 KB halt with a red blink.
- **Dual BLE roles** — NimBLE-Arduino runs Central and Peripheral simultaneously on the single radio via time-slicing.
- **Connection intervals** — Both links target 7.5–15 ms (`minInterval=6, maxInterval=12` in BLE units). The Shield parameter update fires 1 s after CCCD write to avoid disrupting Android's service-discovery sequence.
- **Key-release pulse** — Keyboard-translated buttons get a forced 30 ms key-up (`CFG_KB_PULSE_MS`) so the Shield doesn't auto-repeat them on hold.
- **Bounce guard** — `CFG_BOUNCE_GUARD_ACTION_MS` suppresses the same usage code arriving too soon after key-up (currently **0 ms**, disabled).  Increase if action buttons double-fire.  Nav keys always skip it.
- **Nav key fast-path** — Navigation keys (0x0042–0x0045) skip the bounce guard and release immediately on all-zero idle reports instead of waiting for the 50 ms all-zero guard (`CFG_ALL_ZERO_GUARD_MS`).  Identical-report hold dedup still applies to all keys.
- **Power / volume over BLE** — By default Power (`CFG_IGNORE_TIVO_POWER_BLE=1`) and Vol+/Vol−/Mute (`CFG_IGNORE_TIVO_VOLUME_BLE=1`) are **not** forwarded to the Shield (use TiVo IR). Set either flag to `0` to restore BLE pass-through for that group. If Power is forwarded, it uses a forced 30 ms release pulse.
- **NVS namespaces** — `tivo` (address + `trusted` after 5 s link), `shield` (address after CCCD), `keymap` (custom remaps — storage only; no runtime UI yet). NimBLE bond keys use `CONFIG_BT_NIMBLE_NVS_PERSIST` in `platformio.ini`.
- **Boot reconnect** — On power-up with both bonds stored, TiVo central reconnect is deferred for `CFG_SHIELD_RECONNECT_BOOT_MS` (8 s) so the Shield can reconnect while peripheral advertising is still running. TiVo connect pauses advertising briefly; once TiVo HID setup finishes, advertising resumes if the Shield is not yet linked. Tune the delay in `config.h` if your Shield needs more time after wake.
- **Shield drop while TiVo linked** — After a mid-session Shield disconnect, peripheral advertising uses slow intervals (`CFG_ADV_SLOW_MIN_INTERVAL` / `CFG_ADV_SLOW_MAX_INTERVAL`, 100–300 ms) while the TiVo central link is still up, so fast 20–40 ms re-advertise does not starve the remote. When TiVo also drops, advertising returns to the fast pairing intervals.
- **LED** — Non-blocking state machine driven by `ledTick()` in `loop()`. Priority: Activity (white) > base pattern. Purple slow blink = Shield pairing; deep orange double-flash = TiVo pairing; green = ready. Global brightness controlled by `CFG_LED_BRIGHTNESS` in `config.h`.
- **Shield dropout debug** — `CFG_SHIELD_DEBUG=1` in `config.h` logs `[HID-DBG]` on connect/disconnect (uptime, conn interval, supervision timeout, advertising state), TiVo central pause/resume, fast-params timing, 30 s heartbeat, and ESP reset reason at boot. Set `CFG_SHIELD_DEBUG` to `0` to silence.

---

## License

[MIT](LICENSE) — do whatever you like with it.

---

## Support

If this project saved you a broken remote or a pile of HDMI switches, consider **[buying me a coffee](https://www.buymeacoffee.com/wnt2fly)** — same link as my [F1-Info-display-Enhanced](https://github.com/Wnt2Fly/F1-Info-display-Enhanced) project.

