#!/system/bin/sh
set -e

SCRIPTDIR=$(dirname "$0")
cd "$SCRIPTDIR"
mkdir -p efisp

echo "=== Safe EFISP Loader + Fake Re-lock (Android) ==="
echo ""

if [ ! -f images/abl.img ]; then
  echo "ERROR: images/abl.img not found"
  echo "Place your abl partition image at images/abl.img"
  exit 1
fi

echo "[1/2] Extracting Android LinuxLoader from abl.img..."
./bin/extractfv -o ./ images/abl.img

if [ ! -f LinuxLoader.efi ]; then
  echo "ERROR: extractfv produced no LinuxLoader.efi"
  exit 1
fi

mv -f LinuxLoader.efi ABL_original.efi
echo "  Original ABL saved as ABL_original.efi"

echo ""
echo "[2/2] Applying gbl_root_canoe fake re-lock patch..."
if ! ./bin/patch_abl ABL_original.efi efisp/boot.efi; then
  echo ""
  echo "ERROR: gbl patch_abl (fake re-lock) failed"
  exit 1
fi

if [ ! -f efisp/boot.efi ]; then
  echo "ERROR: patch_abl produced no output"
  exit 1
fi
echo "  Fake re-lock applied: efisp/boot.efi"

echo ""
echo "========================================"
echo "Done. Outputs:"
echo "  efisp/boot.efi   - safe EFISP Android loader + fake re-lock"
echo "                     (AVB follows the device's real unlock state)"
echo "  ABL_original.efi   - original unpatched ABL (backup)"
echo "========================================"
