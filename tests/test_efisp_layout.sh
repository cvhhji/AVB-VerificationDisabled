#!/usr/bin/env bash
set -eu

for platform in linux android windows; do
    root="targets/toolkit_${platform}/resources"
    entries="$root/efisp/BOOTENTRIES"

    test -f "$entries"
    test "$(tr -d '\r\n' < "$entries")" = "Android:boot.efi"

    if [ "$platform" = windows ]; then
        script="$root/build.bat"
        grep -Fq 'efisp\boot.efi' "$script"
        grep -Fq 'if not exist efisp mkdir efisp' "$script"
    else
        script="$root/build.sh"
        grep -Fq 'efisp/boot.efi' "$script"
        grep -Fq 'mkdir -p efisp' "$script"
    fi

    if grep -Fq 'ABL_patched.efi' "$script"; then
        echo "FAIL: stale non-EFISP output path in $script"
        exit 1
    fi
done

echo "PASS: EFISP toolkit layout is consistent"
