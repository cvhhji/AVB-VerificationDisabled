#!/usr/bin/env bash
# test_abl_patch.sh - Batch-test patch_abl_avb against multiple ABL samples.
#
# Usage:
#   tests/test_abl_patch.sh [patcher_path] [samples_dir]
#
# Place ABL ELF files (*.elf, *.bin) in tests/samples/ to test against.
# Each sample is patched to a temp file; exit code reflects overall result.
set -euo pipefail

PATCHER="${1:-abl_patcher/patch_abl_avb}"
SAMPLES_DIR="${2:-tests/samples}"
PASS=0
FAIL=0
SKIP=0

echo "=== ABL AVB Patcher Test Suite ==="
echo "Patcher: $PATCHER"
echo "Samples: $SAMPLES_DIR"
echo ""

# Test 1: patcher exists and is executable
if [ ! -x "$PATCHER" ]; then
    echo "FAIL: patcher not found or not executable: $PATCHER"
    exit 1
fi
echo "[PASS] patcher exists and is executable"
PASS=$((PASS + 1))

# Test 2: no arguments should fail with usage
if "$PATCHER" >/dev/null 2>&1; then
    echo "[FAIL] patcher succeeded with no arguments (expected failure)"
    FAIL=$((FAIL + 1))
else
    echo "[PASS] patcher correctly rejects missing arguments"
    PASS=$((PASS + 1))
fi

# Test 3: batch test against ABL samples
if [ ! -d "$SAMPLES_DIR" ]; then
    echo ""
    echo "[SKIP] samples directory does not exist: $SAMPLES_DIR"
    echo "       Place ABL ELF files there to run patch tests."
    SKIP=$((SKIP + 1))
else
    shopt -s nullglob
    samples=("$SAMPLES_DIR"/*.elf "$SAMPLES_DIR"/*.bin)
    shopt -u nullglob

    if [ ${#samples[@]} -eq 0 ]; then
        echo ""
        echo "[SKIP] no .elf or .bin samples found in $SAMPLES_DIR"
        SKIP=$((SKIP + 1))
    else
        work_dir=$(mktemp -d)
        trap 'rm -rf "$work_dir"' EXIT

        for sample in "${samples[@]}"; do
            name=$(basename "$sample")
            output="$work_dir/${name}.patched"
            echo -n "  $name ... "
            if "$PATCHER" "$sample" "$output" >/dev/null 2>&1; then
                if [ -f "$output" ] && [ -s "$output" ]; then
                    in_size=$(stat -c%s "$sample" 2>/dev/null || wc -c < "$sample")
                    out_size=$(stat -c%s "$output" 2>/dev/null || wc -c < "$output")
                    echo "PASS (${in_size} -> ${out_size} bytes)"
                    PASS=$((PASS + 1))
                else
                    echo "FAIL (output missing or empty)"
                    FAIL=$((FAIL + 1))
                fi
            else
                rc=$?
                if [ "$rc" -eq 2 ]; then
                    echo "WARN (no patches applied - different ABL layout?)"
                    SKIP=$((SKIP + 1))
                else
                    echo "FAIL (patcher error, rc=$rc)"
                    FAIL=$((FAIL + 1))
                fi
            fi
        done
    fi
fi

echo ""
echo "=== Results: PASS=$PASS FAIL=$FAIL SKIP=$SKIP ==="
[ "$FAIL" -eq 0 ]
