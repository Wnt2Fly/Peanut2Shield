Peanut2Shield — Reflash guide (Linux)
======================================

Hardware: Waveshare ESP32-S3-Zero in the Peanut2Shield case
Firmware: see VERSION.txt

-----------------------------------------------------------------------------
ONE-TIME SETUP
-----------------------------------------------------------------------------

Install Python 3 and esptool. Examples:

  Debian / Ubuntu / Raspberry Pi OS:
    sudo apt update
    sudo apt install python3 python3-pip
    pip3 install --user esptool

  Fedora:
    sudo dnf install python3 python3-pip
    pip3 install --user esptool

Serial port access (avoid sudo for every flash):

    sudo usermod -aG dialout "$USER"
    # log out and back in (or reboot)

-----------------------------------------------------------------------------
FIND THE SERIAL PORT
-----------------------------------------------------------------------------

1. Plug the board in with a USB-C data cable.
2. Run:
      ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
   Common names: /dev/ttyACM0 (native USB on ESP32-S3-Zero)

-----------------------------------------------------------------------------
OPTION A — UPDATE (recommended)
-----------------------------------------------------------------------------

Shield and TiVo pairing usually survives this flash.

  cd /path/to/usb-drive
  chmod +x flash-update.sh    # once, if needed
  ./flash-update.sh

Press Enter for the default port, or type e.g. /dev/ttyACM0.

-----------------------------------------------------------------------------
OPTION B — FULL FLASH
-----------------------------------------------------------------------------

Use if update fails or the board never ran Peanut2Shield.

  ./flash-full.sh

You will likely need to pair Shield and TiVo again.

-----------------------------------------------------------------------------
TROUBLESHOOTING
-----------------------------------------------------------------------------

"Permission denied" on /dev/ttyACM0
  → Add your user to the dialout group (see above).

"Failed to connect"
  → Unplug/replug USB, press RESET, retry.
  → Hold BOOT, tap RESET, release BOOT (download mode), retry.

esptool not found after pip install --user
  → Run:  python3 -m esptool ...
  → Or add ~/.local/bin to PATH.

-----------------------------------------------------------------------------
REPACK THE USB KIT (MAINTAINER)
-----------------------------------------------------------------------------

From the project root on Linux:

  chmod +x usb-drive/pack-usb-drive.sh
  ./usb-drive/pack-usb-drive.sh

Then copy the whole usb-drive folder to the stick.
