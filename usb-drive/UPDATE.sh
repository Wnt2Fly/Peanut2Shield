#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo
echo "Peanut2Shield firmware update"
echo "=============================="
cat VERSION.txt 2>/dev/null || true
echo
echo "  1  Normal update (keeps pairing)"
echo "  2  Full fix (yellow LED / new board)"
echo "  Q  Quit"
echo
read -r -p "Pick 1, 2, or Q: " choice
case "${choice^^}" in
  1) exec ./tools/linux/flash-update.sh ;;
  2) exec ./tools/linux/flash-full.sh ;;
  *) exit 0 ;;
esac
