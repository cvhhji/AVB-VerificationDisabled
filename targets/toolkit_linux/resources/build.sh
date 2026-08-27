#!/usr/bin/env bash
set -euo pipefail

SCRIPTDIR=$(dirname "$0")
cd "$SCRIPTDIR"

echo "=== ABL Toolkit: Fake Re-lock + AVB Disable (Linux) ==="
echo ""

if [ ! -f images/abl.img ]; then
  echo "ERROR: images/abl.img not found"
  echo "Place your abl partition image at images/abl.img"
  exit 1
fi

echo "[1/5] Extracting ABL ELF from abl.img..."
./bin/extractfv -o ./ images/abl.img

if [ ! -f LinuxLoader.efi ]; then
  echo "ERROR: extractfv produced no LinuxLoader.efi"
  exit 1
fi

mv -f LinuxLoader.efi ABL_original.efi
echo "  Original ABL saved as ABL_original.efi"

echo ""
echo "[2/5] Applying fake re-lock patch (gbl patch_abl)..."
if ! ./bin/patch_abl ABL_original.efi ABL_relocked.efi; then
  echo ""
  echo "ERROR: gbl patch_abl (fake re-lock) failed"
  exit 1
fi

if [ ! -f ABL_relocked.efi ] || [ ! -s ABL_relocked.efi ]; then
  echo "ERROR: patch_abl produced no output"
  exit 1
fi
echo "  Fake re-lock applied: ABL_relocked.efi"

echo ""
echo "[3/5] Applying AVB verification disable patch..."
if ! ./bin/patch_abl_avb ABL_relocked.efi ABL_patched.efi; then
  echo ""
  echo "ERROR: patch_abl_avb (AVB disable) failed"
  exit 1
fi

if [ ! -f ABL_patched.efi ] || [ ! -s ABL_patched.efi ]; then
  echo "ERROR: patch_abl_avb produced no output"
  exit 1
fi
echo "  AVB disable applied: ABL_patched.efi"

echo ""
echo "[4/5] Repacking into abl.img (in-place replace)..."
if ! ./bin/abl_pack images/abl.img ABL_patched.efi abl_patched.img; then
  echo ""
  echo "ERROR: abl_pack failed to repack"
  exit 1
fi

echo ""
echo "[5/5] Done."
echo ""
echo "========================================"
echo "Outputs:"
echo "  abl_patched.img     - final abl.img (fake re-lock + AVB disabled)"
echo "  ABL_patched.efi     - patched ABL ELF (for analysis)"
echo "  ABL_relocked.efi    - after fake re-lock only (intermediate)"
echo "  ABL_original.efi    - original unpatched ABL (backup, do NOT flash)"
echo ""
echo "Flash abl_patched.img to the abl partition:"
echo "  dd if=abl_patched.img of=/dev/block/by-name/abl"
echo "========================================"
