#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

echo "Building Peanut2Shield..."
pio run -e waveshare-esp32-s3-zero

BUILD=".pio/build/waveshare-esp32-s3-zero"
KIT="usb-drive"
PIO="${HOME}/.platformio"

BOOTAPP=""
if [[ -f "${KIT}/pack/vendor/boot_app0.bin" ]]; then
  BOOTAPP="${KIT}/pack/vendor/boot_app0.bin"
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
  echo "WARNING: boot_app0.bin not found — full flash will not work."
fi

mkdir -p "${KIT}/firmware/update" "${KIT}/firmware/full" "${KIT}/pack/vendor"
cp -f "${BUILD}/firmware.bin" "${KIT}/firmware/update/"
cp -f "${BUILD}/firmware.bin" "${KIT}/firmware/full/"
cp -f "${BUILD}/bootloader.bin" "${KIT}/firmware/full/"
cp -f "${BUILD}/partitions.bin" "${KIT}/firmware/full/"
if [[ -n "${BOOTAPP}" ]]; then
  cp -f "${BOOTAPP}" "${KIT}/firmware/full/boot_app0.bin"
  cp -f "${BOOTAPP}" "${KIT}/pack/vendor/boot_app0.bin"
  echo "Building combined full-flash image..."
  if pio pkg exec --package tool-esptoolpy -- esptool.py --chip esp32s3 merge_bin \
      -o "${KIT}/firmware/full/combined.bin" --flash_size 4MB \
      0x0 "${BUILD}/bootloader.bin" \
      0x8000 "${BUILD}/partitions.bin" \
      0xe000 "${KIT}/firmware/full/boot_app0.bin" \
      0x10000 "${BUILD}/firmware.bin"; then
    echo "combined.bin OK"
  else
    echo "WARNING: could not build combined.bin"
    rm -f "${KIT}/firmware/full/combined.bin"
  fi
fi

FW_VER=$(grep '#define CFG_FIRMWARE_VERSION' src/config.h | sed -n 's/.*"\([^"]*\)".*/\1/p')

{
  echo "Peanut2Shield USB update kit"
  echo "Firmware: ${FW_VER}"
  echo "Built: $(date -Iseconds 2>/dev/null || date)"
  echo "Board: waveshare-esp32-s3-zero"
  echo "Source: $(pwd)"
} > "${KIT}/VERSION.txt"

chmod +x "${KIT}/UPDATE.sh" "${KIT}/tools/linux/"*.sh 2>/dev/null || true

echo
echo "Done. Copy ${KIT}/ to the USB stick."
ls -la "${KIT}/firmware/update/" "${KIT}/firmware/full/"
