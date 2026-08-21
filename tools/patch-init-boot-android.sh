#!/system/bin/sh
set -eu
fail(){ echo "错误：$*" >&2; exit 1; }
usage(){ echo "用法：$0 --input <镜像> --output <新镜像> --loader <avbinit> --module <avb_interceptor.ko> [--magiskboot <路径>]" >&2; }
hash(){ if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else toybox sha256sum "$1"; fi | awk '{print $1}'; }
input= output= loader= module=; magiskboot=${MAGISKBOOT:-magiskboot}
while [ $# -gt 0 ]; do
 case "$1" in
  --input) input=${2:-}; shift 2;; --output) output=${2:-}; shift 2;;
  --loader) loader=${2:-}; shift 2;; --module) module=${2:-}; shift 2;;
  --magiskboot) magiskboot=${2:-}; shift 2;; -h|--help) usage; exit 0;;
  *) usage; fail "未知参数 $1";; esac
done
[ -f "$input" ] && [ -f "$loader" ] && [ -f "$module" ] && [ -n "$output" ] || { usage; exit 2; }
[ ! -e "$output" ] || fail "输出已存在：$output"
command -v "$magiskboot" >/dev/null 2>&1 || [ -x "$magiskboot" ] || fail "找不到 magiskboot"
work=${TMPDIR:-/data/local/tmp}/avb-vd-$$; trap 'rm -rf "$work"' EXIT INT TERM; mkdir -p "$work/image" "$work/assets" "$work/extract"
cp "$loader" "$work/assets/avbinit"; cp "$module" "$work/assets/avb_interceptor.ko"
cd "$work/image"; "$magiskboot" unpack "$input" || fail "无法解包镜像"; [ -f ramdisk.cpio ] || fail "镜像不含 ramdisk"
"$magiskboot" cpio ramdisk.cpio "exists init" || fail "ramdisk 不含 /init"
for e in init.next avb_interceptor.ko avb_interceptor.meta; do ! "$magiskboot" cpio ramdisk.cpio "exists $e" >/dev/null 2>&1 || fail "已存在 /$e"; done
"$magiskboot" cpio ramdisk.cpio "extract init ../extract/original-init"
{
 echo format=3; echo project=AVB-VerificationDisabled
 echo original_init_sha256=$(hash "$work/extract/original-init")
 echo loader_sha256=$(hash "$work/assets/avbinit")
 echo module_sha256=$(hash "$work/assets/avb_interceptor.ko")
} > "$work/assets/avb_interceptor.meta"
"$magiskboot" cpio ramdisk.cpio "mv init init.next" "add 0755 init ../assets/avbinit" "add 0644 avb_interceptor.ko ../assets/avb_interceptor.ko" "add 0644 avb_interceptor.meta ../assets/avb_interceptor.meta"
"$magiskboot" repack "$input" "$work/candidate.img" || fail "重打包失败"
cp "$work/candidate.img" "$output"; chmod 0644 "$output"; echo "完成：$output（未自动刷写）"
