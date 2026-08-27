#!/system/bin/sh
set -e

SCRIPTDIR=$(dirname "$0")
cd "$SCRIPTDIR"

echo "=== ABL Toolkit: AVB Disable + Fake Re-lock (Android) ==="
echo ""

if [ ! -f images/abl.img ]; then
  echo "ERROR: images/abl.img not found"
  echo "Place your abl partition image at images/abl.img"
  exit 1
fi

echo "[1/3] Extracting ABL ELF from abl.img..."
./bin/extractfv -o ./ images/abl.img

if [ ! -f LinuxLoader.efi ]; then
  echo "ERROR: extractfv produced no LinuxLoader.efi"
  exit 1
fi

mv -f LinuxLoader.efi ABL_original.efi
echo "  Original ABL saved as ABL_original.efi"

echo ""
echo "[2/3] Applying AVB verification disable patch..."
if ! ./bin/patch_abl_avb ABL_original.efi ABL_avb.efi; then
  echo ""
  echo "ERROR: patch_abl_avb (AVB disable) failed"
  exit 1
fi

if [ ! -f ABL_avb.efi ]; then
  echo "ERROR: patch_abl_avb produced no output"
  exit 1
fi
echo "  AVB disable applied: ABL_avb.efi"

echo ""
echo "[3/3] Applying fake re-lock patch (gbl patch_abl)..."
if ! ./bin/patch_abl ABL_avb.efi ABL_patched.efi; then
  echo ""
  echo "ERROR: gbl patch_abl (fake re-lock) failed"
  exit 1
fi

if [ ! -f ABL_patched.efi ]; then
  echo "ERROR: patch_abl produced no output"
  exit 1
fi
echo "  Fake re-lock applied: ABL_patched.efi"

echo ""
echo "========================================"
echo "Done. Outputs:"
echo "  ABL_patched.efi   - final ABL (AVB disabled + fake re-lock)"
echo "  ABL_avb.efi        - after AVB disable only (intermediate)"
echo "  ABL_original.efi   - original unpatched ABL (backup)"
echo "========================================"
