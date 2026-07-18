#!/usr/bin/env bash
set -euo pipefail
KIT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$KIT"

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

if [[ ! -f firmware/update/firmware.bin ]]; then
  echo "Missing firmware/update/firmware.bin"
  echo "Run pack/pack.bat or pack/pack.sh on the build PC first."
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
  write_flash 0x10000 firmware/update/firmware.bin

echo
echo "SUCCESS. Press the RESET button on the board once, then unplug from PC."
echo "Power from a WALL USB adapter — not the Nvidia Shield USB port."
echo "(Charge-only cable OK for Shield power only; data cable can hang the board.)"
echo "LED should BLINK purple, or green if already paired. Solid purple = hung."
echo
