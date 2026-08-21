#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
bash -n tools/*.sh; sh -n tools/patch-init-boot-android.sh
! grep -q '^hash()' tools/patch-init-boot-android.sh
grep -q 'sha256_file.*original-init' tools/patch-init-boot-android.sh
grep -q 'sha256_file.*avbinit' tools/patch-init-boot-android.sh
grep -q 'sha256_file.*avb_interceptor\.ko' tools/patch-init-boot-android.sh
grep -q 'AVB_VBMETA_FLAGS_OFFSET 120U' module/vbmeta_proxy.c
grep -q 'AVB_VBMETA_VERIFICATION_DISABLED_BYTE_MASK 0x02U' module/vbmeta_proxy.c
grep -q 'androidboot.verifiedbootstate = \\"orange\\"' module/bootconfig_proxy.c
! grep -RIE 'dsu_detect|dsu_config|selinux_enforce|permissive_prefix|avb_enforce' module loader
if find . -type f ! -path './.git/*' ! -path './tests/static-check.sh' -print0 | xargs -0 grep -IE 'ghp_|github_pat_'; then exit 1; fi
! grep -RIE '\b(filp_open|dentry_open|kernel_read|kernel_write|vfs_write|blkdev_get)\b' module
echo 'static checks passed'
