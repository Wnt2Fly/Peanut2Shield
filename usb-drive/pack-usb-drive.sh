#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

echo "Building Peanut2Shield..."
pio run -e waveshare-esp32-s3-zero

BUILD=".pio/build/waveshare-esp32-s3-zero"
PIO="${HOME}/.platformio"

BOOTAPP=""
if [[ -f usb-drive/vendor/boot_app0.bin ]]; then
  BOOTAPP="usb-drive/vendor/boot_app0.bin"
else
  for d in "${PIO}"/packages/framework-arduinoespressif32*; do
    if [[ -f "${d}/tools/partitions/boot_app0.bin" ]]; then
      BOOTAPP="${d}/tools/partitions/boot_app0.bin"
      break
    fi
  done
fi

if [[ ! -f "${BUILD}/firmware.bin" ]]; then
  echo "Missing ${BUILD}/firmware.bin"
  exit 1
fi

if [[ -z "${BOOTAPP}" ]]; then
  echo "WARNING: boot_app0.bin not found — flash-full will not work; flash-update is OK."
fi

mkdir -p usb-drive/firmware usb-drive/firmware-full
cp -f "${BUILD}/firmware.bin" usb-drive/firmware/
cp -f "${BUILD}/firmware.bin" usb-drive/firmware-full/
cp -f "${BUILD}/bootloader.bin" usb-drive/firmware-full/
cp -f "${BUILD}/partitions.bin" usb-drive/firmware-full/
if [[ -n "${BOOTAPP}" ]]; then
  cp -f "${BOOTAPP}" usb-drive/firmware-full/boot_app0.bin
fi

{
  echo "Peanut2Shield USB flash kit"
  echo "Built: $(date -Iseconds 2>/dev/null || date)"
  echo "Board: waveshare-esp32-s3-zero"
  echo "Source: $(pwd)"
} > usb-drive/VERSION.txt

chmod +x usb-drive/flash-update.sh usb-drive/flash-full.sh 2>/dev/null || true

echo
echo "Done. Copy the usb-drive folder to the USB stick."
ls -la usb-drive/firmware/ usb-drive/firmware-full/
