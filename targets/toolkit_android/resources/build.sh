#!/system/bin/sh
set -e

SCRIPTDIR=$(dirname "$0")
cd "$SCRIPTDIR"

echo "=== AVB-VerificationDisabled ABL Toolkit (Android) ==="
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
if ! ./bin/patch_abl_avb ABL_original.efi ABL_patched.efi; then
  echo ""
  echo "ERROR: patch_abl_avb failed"
  echo "The ABL may use a different code layout than expected."
  exit 1
fi

if [ ! -f ABL_patched.efi ]; then
  echo "ERROR: patch_abl_avb produced no output"
  exit 1
fi

echo ""
echo "[3/3] Done."
echo ""
echo "========================================"
echo "Outputs:"
echo "  ABL_patched.efi   - ABL with AVB verification disabled (BL-stage)"
echo "  ABL_original.efi  - original unpatched ABL (backup, do NOT flash)"
echo ""
echo "Flash ABL_patched.efi to the abl partition (repack if needed)."
echo "Combine with gbl_root_canoe fake-relock for locked+green state."
echo "========================================"
