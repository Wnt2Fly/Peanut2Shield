#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo
echo "Peanut2Shield - firmware UPDATE (pairing usually kept)"
echo "======================================================"
echo

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found. Install Python 3, then run:  pip3 install esptool"
  echo "  Debian/Ubuntu: sudo apt install python3 python3-pip"
  exit 1
fi

if ! python3 -m pip show esptool >/dev/null 2>&1; then
  echo "Installing esptool..."
  python3 -m pip install --user esptool
fi

if [[ ! -f firmware/firmware.bin ]]; then
  echo "Missing firmware/firmware.bin"
  echo "Run pack-usb-drive.bat or pack-usb-drive.sh on the build PC first."
  exit 1
fi

DEFAULT_PORT="/dev/ttyACM0"
read -r -p "Enter serial port [${DEFAULT_PORT}]: " PORT
PORT="${PORT:-$DEFAULT_PORT}"

echo
echo "Flashing firmware.bin to ${PORT} ..."
echo "(If permission denied: sudo usermod -aG dialout \"\$USER\" and log out/in)"
echo

python3 -m esptool --chip esp32s3 --port "${PORT}" --baud 460800 \
  write_flash 0x10000 firmware/firmware.bin

echo
echo "SUCCESS. Press the RESET button on the board once, then unplug from PC."
echo
