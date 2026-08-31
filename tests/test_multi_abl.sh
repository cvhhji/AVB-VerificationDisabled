#!/usr/bin/env bash
# test_multi_abl.sh - Download ABL samples from gbl_root_canoe, extract ELF,
# and run patch_abl_avb against each one.
set -eu

PATCHER="${1:-abl_patcher/patch_abl_avb}"
WORK_DIR="${2:-/tmp/abl_multi_test}"
GBL_RAW="https://raw.githubusercontent.com/cvhhji/gbl_root_canoe/main"

DEVICES="CPH2841 OPD2513 PLK110 PLR110 PLZ110 PMA120 myron nezha nezha_global pandora popsicle pudding"

PASS=0
FAIL=0
SKIP=0

echo "=== Multi-ABL Patch Test ==="
echo "Patcher: $PATCHER"
echo "Samples: ${GBL_RAW}/ablrepo/"
echo ""

if [ ! -x "$PATCHER" ]; then
    echo "FAIL: patcher not found: $PATCHER"
    exit 1
fi

# Build extractfv
echo "[1/3] Building extractfv..."
mkdir -p "$WORK_DIR"
curl -sL "$GBL_RAW/submodules/ablfvextractor/extractfv.c" -o "$WORK_DIR/extractfv.c"
if [ ! -s "$WORK_DIR/extractfv.c" ]; then
    echo "FAIL: cannot download extractfv.c"
    exit 1
fi
gcc -O2 -o "$WORK_DIR/extractfv" "$WORK_DIR/extractfv.c" -llzma 2>/dev/null || \
gcc -O2 -o "$WORK_DIR/extractfv" "$WORK_DIR/extractfv.c" -llzma -I/usr/include 2>/dev/null || {
    echo "FAIL: cannot build extractfv (missing liblzma?)"
    exit 1
}
echo "extractfv built OK"

# Test each device
echo ""
echo "[2/3] Patching ABL samples..."
for device in $DEVICES; do
    printf "  %-15s " "$device:"
    img="$WORK_DIR/${device}.img"
    elf_dir="$WORK_DIR/${device}_out"

    # Download abl.img
    if ! curl -sL "$GBL_RAW/ablrepo/$device/abl.img" -o "$img" 2>/dev/null || [ ! -s "$img" ]; then
        echo "SKIP (download failed)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Extract PE/ELF
    rm -rf "$elf_dir"
    if ! "$WORK_DIR/extractfv" -o "$elf_dir" -e pe32 "$img" >/dev/null 2>&1; then
        echo "SKIP (extract failed)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Find extracted EFI
    elf=$(find "$elf_dir" -type f -name "*.efi" 2>/dev/null | head -1)
    if [ -z "$elf" ] || [ ! -s "$elf" ]; then
        echo "SKIP (no EFI extracted)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Run patcher
    output="$WORK_DIR/${device}_patched.efi"
    log="$WORK_DIR/${device}_patch.log"
    if "$PATCHER" "$elf" "$output" >"$log" 2>&1; then
        if [ -f "$output" ] && [ -s "$output" ]; then
            in_size=$(wc -c < "$elf" | tr -d ' ')
            out_size=$(wc -c < "$output" | tr -d ' ')
            echo "PASS (${in_size} -> ${out_size})"
            PASS=$((PASS + 1))
        else
            echo "FAIL (empty output)"
            FAIL=$((FAIL + 1))
        fi
    else
        rc=$?
        if [ "$rc" -eq 2 ]; then
            candidates=$(sed -n 's/.*refusing ambiguous match: \([0-9][0-9]*\) candidate.*/\1/p' "$log" | tail -1)
            if [ -n "$candidates" ]; then
                echo "WARN (ambiguous: $candidates candidates)"
            else
                echo "WARN (no verified candidate)"
            fi
            sed -n '/\[short_circuit\]/p; /  -> function at/p' "$log" | sed 's/^/      /'
            SKIP=$((SKIP + 1))
        else
            echo "FAIL (rc=$rc)"
            sed 's/^/      /' "$log"
            FAIL=$((FAIL + 1))
        fi
    fi
done

echo ""
echo "[3/3] Results: PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
if [ "$PASS" -eq 0 ]; then
    echo "FAIL: no ABL sample produced a verified patch"
    exit 1
fi
[ "$FAIL" -eq 0 ]
