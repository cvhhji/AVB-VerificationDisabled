# AVB-VerificationDisabled

面向 arm64 Android GKI 的 AVB 校验关闭工具集，支持 **init 阶段**和 **BL (bootloader) 阶段**两种工作模式。

从 [yangFenTuoZi/DSU-Permissive](https://github.com/yangFenTuoZi/DSU-Permissive)
的 AVB 路径提取，删除了 DSU 条件、SELinux permissive 与运行时配置，供已经能启动修补后
`boot/init_boot/vendor_boot/dtbo` 和自定义内核的外部启动链使用。

> **高风险实验项目。** 构建成功不等于适配厂商内核。刷写前必须保留当前槽位镜像、另一槽位或
> recovery/fastboot 救砖路径。本项目当前没有真机启动验证。

## 两种工作模式

### 1. Init 阶段（原有）

ramdisk 中的 `avbinit` 在原始 `/init` 之前加载 `avb_interceptor.ko`，模块拦截 PID 1 的
`/proc/bootconfig` 和 vbmeta 分区读取，使 `libfs_avb` 识别为 `VerificationDisabled` 从而跳过
system/vendor/odm 等分区的 hashtree 校验。

### 2. BL 阶段（新增）

`abl_patcher/patch_abl_avb` 对高通 ABL ELF 打补丁，在 bootloader 层面短路 AVB 校验、
强制 `verifiedbootstate=green`，使修改过的 boot/init_boot 镜像能通过 ABL 校验并正常启动。
与 [gbl_root_canoe](https://github.com/cvhhji/gbl_root_canoe) 假回锁配合，实现
**假回锁状态下关闭 AVB 且正常开机，且不被系统发现 BL 已解锁**。

## 原理

### Init 阶段

1. ramdisk 中的 `avbinit` 在原始 `/init` 之前加载 `avb_interceptor.ko`。
2. first-stage PID 1 读取 `/proc/bootconfig` 时，代理在仅该文件实例的读取视图前置：
   `androidboot.verifiedbootstate = "orange"`。
3. PID 1 读取顶层 `vbmeta[_a|_b]` 时，仅修改返回用户缓冲区：确认 `AVB0` 后，将
   `AvbVBMetaImageHeader.flags` 的绝对偏移 123 OR `0x02`。
4. AOSP `libfs_avb` 可把该 AVB handle 识别为 `VerificationDisabled`，从而跳过同一 handle
   管理的 system/vendor/odm 等分区 Hashtree。
5. PID 1 第一次 exec `/system/bin/init` 时注销 Hook；另有 120 秒安全超时。

磁盘 vbmeta、页缓存以及其他进程读取视图不会被修改。

### BL 阶段

对 ABL ELF 应用三类补丁：

| 补丁 | 作用 |
|------|------|
| 短路 AVB 校验入口 | 校验函数直接返回 `EFI_SUCCESS`，跳过 boot/init_boot/vendor_boot/dtbo 校验 |
| 强制 verifiedbootstate=green | 将 orange/yellow/red 字符串引用重定向到 green，隐藏解锁状态 |
| NOP 错误分支 | 消除跳转到 red/error 状态的条件分支，防止启动失败画面 |

结果：ABL 报告 `device_state=locked` + `verifiedbootstate=green`（与真锁完全一致），
但不执行任何校验，修改过的镜像可正常启动。

### 假回锁 + Green 模式

当与 gbl_root_canoe 假回锁配合时，使用 `--green-mode` 修补 init_boot：
- ramdisk 中放置 `/avb_keep_green` 标志文件
- `avbinit` 检测到后向模块传递 `avb_keep_green=1`
- 模块跳过 orange 注入，保持 ABL 提供的 green 状态
- vbmeta flags 仍被修改（`\|= 0x02`），`libfs_avb` 跳过校验
- 系统看到 `locked + green`，无法检测 BL 已解锁

## 明确边界

- 不绕过 bootloader 对 boot/init_boot/vendor_boot/dtbo 或自定义内核的加载验证
  （BL 阶段补丁可解决此问题）。
- 不修改磁盘 vbmeta，不签名镜像，不修改 KeyMint/TEE 证明结果。
- 依赖 first-stage `fs_mgr` 从 `/proc/bootconfig` 获取 `verifiedbootstate`，且重复键采用首项。
- 目前匹配常见 `/dev/block/{by-name,bootdevice/by-name}/vbmeta[_a|_b]`。
- 当前默认和首要构建目标是 `android16-6.12`；结构保留其他 GKI KMI target。
- 目标设备必须允许加载匹配 KMI/vermagic/签名策略的 LKM，并启用 kprobe 与模块支持。

## 构建

### Init 阶段模块 + loader

本地需要 Android DDK 命令 `ddk`、LLVM、Bash：

```bash
tools/build.sh                         # 默认 android16-6.12
tools/build.sh --target android16-6.12
```

### BL 阶段 ABL 补丁工具

```bash
make abl_patcher
# 或
cd abl_patcher && make
```

仅需主机 gcc/clang，不需要 Android NDK。

GitHub Actions 的 `Build android16-6.12` 工作流会安装官方 Android DDK CLI、执行静态测试并上传
KO、loader、Android 修补脚本、静态 magiskboot 以及 BL 阶段补丁工具。

## 使用

### Init 阶段（离线修补，不直接刷写）

```bash
tools/patch-init-boot.sh \
  --input init_boot_ksu_patched.img \
  --output init_boot_avb_disabled.img \
  --loader out/avbinit \
  --module out/avb_interceptor-android16-6.12.ko
```

假回锁兼容模式（保持 green，不注入 orange）：

```bash
tools/patch-init-boot.sh \
  --input init_boot_ksu_patched.img \
  --output init_boot_avb_green.img \
  --loader out/avbinit \
  --module out/avb_interceptor-android16-6.12.ko \
  --green-mode
```

刷写前运行 `tools/verify-init-boot.sh`。设备端脚本为 `tools/patch-init-boot-android.sh`；一键脚本只有
显式 `--flash` 或交互确认后才写分区。

### BL 阶段（ABL 补丁）

```bash
# 1. 从 abl.img 提取 ABL ELF（使用 gbl_root_canoe 的 extractfv）
extractfv abl.img abl.elf

# 2. 应用假回锁补丁（gbl_root_canoe）
patch_abl abl.elf abl_relocked.elf

# 3. 应用 BL 阶段 AVB 补丁（本工具）
out/patch_abl_avb abl_relocked.elf abl_final.elf

# 4. 重新打包并刷入 abl 分区
```

详细说明见 [abl_patcher/README.md](abl_patcher/README.md)。

### 完整假回锁 + AVB 关闭启动链

```
ABL (patched: gbl_root_canoe 假回锁 + patch_abl_avb 关闭校验)
  │
  ├─ device_state = locked       (假回锁)
  ├─ verifiedbootstate = green   (BL 补丁强制)
  ├─ AVB 校验 = 跳过             (BL 补丁短路)
  │
  └─ 加载 init_boot (patched: avbinit + KO, --green-mode)
       │
       └─ avbinit 检测 /avb_keep_green → 加载 KO (avb_keep_green=1)
            → KO 修改 vbmeta 读取视图 (flags|=0x02)，不注入 orange
            → userspace libfs_avb 跳过 system/vendor 校验
            → 系统看到 locked + green，正常开机
```

## 来源与许可

核心 bootconfig/vbmeta fops 代理、镜像工具与 loader 基于 DSU-Permissive `0.6.1` 提取和修改；
保留原文件 SPDX `GPL-2.0-only`。BL 阶段 ABL 补丁工具参考 gbl_root_canoe 的模式匹配方法。
新增内容同样采用 GPL-2.0-only。
