#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo
echo "Peanut2Shield - FULL flash (may clear Bluetooth pairings)"
echo "========================================================="
echo
echo "Only use if flash-update.sh failed or the board never worked."
read -r -p "Press Enter to continue or Ctrl+C to cancel..."

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found. Install Python 3, then run:  pip3 install esptool"
  exit 1
fi

if ! python3 -m pip show esptool >/dev/null 2>&1; then
  echo "Installing esptool..."
  python3 -m pip install --user esptool
fi

if [[ ! -f firmware-full/firmware.bin ]]; then
  echo "Missing firmware-full/ files. Run pack-usb-drive on the build PC first."
  exit 1
fi

DEFAULT_PORT="/dev/ttyACM0"
read -r -p "Enter serial port [${DEFAULT_PORT}]: " PORT
PORT="${PORT:-$DEFAULT_PORT}"

echo
echo "Full flash to ${PORT} ..."
echo

python3 -m esptool --chip esp32s3 --port "${PORT}" --baud 460800 \
  write_flash 0x0 firmware-full/bootloader.bin \
  0x8000 firmware-full/partitions.bin \
  0xe000 firmware-full/boot_app0.bin \
  0x10000 firmware-full/firmware.bin

echo
echo "SUCCESS. Press RESET, then re-pair Shield and TiVo if needed."
echo
