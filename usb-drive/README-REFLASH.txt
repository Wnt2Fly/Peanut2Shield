Peanut2Shield — Reflash guide (Windows)
========================================

Hardware: Waveshare ESP32-S3-Zero in the Peanut2Shield case
Firmware file in this kit: see VERSION.txt for build date

-----------------------------------------------------------------------------
ONE-TIME PC SETUP
-----------------------------------------------------------------------------

1. Install Python 3: https://www.python.org/downloads/
   During install, enable "Add python.exe to PATH".

2. Open Command Prompt (Win+R, type cmd, Enter) and run:
      pip install esptool

You only do this once per PC.

-----------------------------------------------------------------------------
FIND THE COM PORT
-----------------------------------------------------------------------------

1. Plug the board into the PC with USB-C.
2. Open Device Manager (Win+X → Device Manager).
3. Expand "Ports (COM & LPT)".
4. Note the port named something like "USB Serial Device (COM19)".
   If nothing appears, try another USB port or cable.

-----------------------------------------------------------------------------
OPTION A — UPDATE (recommended)
-----------------------------------------------------------------------------

Use this when the bridge worked before and you are just installing a new version.
Shield and TiVo Bluetooth pairing usually survives this flash.

1. Double-click flash-update.bat
2. Enter the COM port when asked (e.g. COM19)
3. When it finishes, press RESET on the board once
4. Unplug from PC, plug back into TV power

-----------------------------------------------------------------------------
OPTION B — FULL FLASH
-----------------------------------------------------------------------------

Use if Option A fails, the board won't boot, or this is a brand-new chip never
flashed with Peanut2Shield before.

Warning: may erase stored pairings — you will likely need to pair Shield and
TiVo again (see START-HERE.txt).

1. Double-click flash-full.bat
2. Enter the COM port
3. Press RESET when done

-----------------------------------------------------------------------------
TROUBLESHOOTING
-----------------------------------------------------------------------------

"python is not recognized"
  → Reinstall Python with "Add to PATH" checked, or run pip from "Python 3.x"
    in the Start menu.

"Failed to connect"
  → Unplug/replug USB, press RESET, try again.
  → Hold BOOT, press RESET, release RESET, release BOOT (download mode), retry.

"No serial port"
  → Use a data-capable USB-C cable; try a USB 2.0 port on the PC.

Still stuck?
  → Use the full project + VS Code + PlatformIO (see main README in the repo),
    or ask the person who maintains this USB kit to reflash for you.

-----------------------------------------------------------------------------
FOR THE PERSON WHO BUILDS THIS USB STICK
-----------------------------------------------------------------------------

After changing firmware in the repo, on your dev PC run from the project folder:

  Windows:  usb-drive\pack-usb-drive.bat
  Linux:    chmod +x usb-drive/pack-usb-drive.sh && ./usb-drive/pack-usb-drive.sh

Then copy the entire usb-drive folder onto the USB stick.

Linux flashing: see README-REFLASH-LINUX.txt (flash-update.sh / flash-full.sh).
