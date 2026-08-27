# AVB-VerificationDisabled

面向 arm64 Android GKI 的 AVB 校验关闭工具集，工作在 **init 阶段**。

从 [yangFenTuoZi/DSU-Permissive](https://github.com/yangFenTuoZi/DSU-Permissive)
的 AVB 路径提取，删除了 DSU 条件、SELinux permissive 与运行时配置，供已经能启动修补后
`boot/init_boot/vendor_boot/dtbo` 和自定义内核的外部启动链使用。

> **高风险实验项目。** 构建成功不等于适配厂商内核。刷写前必须保留当前槽位镜像、另一槽位或
> recovery/fastboot 救砖路径。

## 原理

1. ramdisk 中的 `avbinit` 在原始 `/init` 之前加载 `avb_interceptor.ko`。
2. first-stage PID 1 读取 `/proc/bootconfig` 时，代理在仅该文件实例的读取视图前置：
   `androidboot.verifiedbootstate = "orange"`。
3. PID 1 读取顶层 `vbmeta[_a|_b]` 时，仅修改返回用户缓冲区：确认 `AVB0` 后，将
   `AvbVBMetaImageHeader.flags` 的绝对偏移 123 OR `0x02`。
4. AOSP `libfs_avb` 可把该 AVB handle 识别为 `VerificationDisabled`，从而跳过同一 handle
   管理的 system/vendor/odm 等分区 Hashtree。
5. PID 1 第一次 exec `/system/bin/init` 时注销 Hook；另有 120 秒安全超时。

磁盘 vbmeta、页缓存以及其他进程读取视图不会被修改。

### 假回锁 + Green 模式

当与 gbl_root_canoe 假回锁配合时，在 ramdisk 中放置 `/avb_keep_green` 标志文件：
- `avbinit` 检测到后向模块传递 `avb_keep_green=1`
- 模块跳过 orange 注入，保持 ABL 提供的 green 状态
- vbmeta flags 仍被修改（`|= 0x02`），`libfs_avb` 跳过校验
- 系统看到 `locked + green`，无法检测 BL 已解锁

## 明确边界

- 不绕过 bootloader 对 boot/init_boot/vendor_boot/dtbo 或自定义内核的加载验证。
- 不修改磁盘 vbmeta，不签名镜像，不修改 KeyMint/TEE 证明结果。
- 依赖 first-stage `fs_mgr` 从 `/proc/bootconfig` 获取 `verifiedbootstate`，且重复键采用首项。
- 目前匹配常见 `/dev/block/{by-name,bootdevice/by-name}/vbmeta[_a|_b]`。
- 当前默认和首要构建目标是 `android16-6.12`。
- 目标设备必须允许加载匹配 KMI/vermagic/签名策略的 LKM，并启用 kprobe 与模块支持。

## 构建

需要 Android DDK 环境，设置 `KDIR` 后：

```bash
make AVB_DDK_TARGET=android16-6.12
```

GitHub Actions 的 `Build android16-6.12` 工作流会自动构建并上传 KO、loader、Android 修补脚本和静态 magiskboot。

## 使用

设备端修补脚本 `tools/patch-init-boot-android.sh`：

```sh
patch-init-boot-android.sh \
  --input init_boot.img \
  --output init_boot_avb_disabled.img \
  --loader avbinit \
  --module avb_interceptor-android16-6.12.ko
```

## 来源与许可

核心 bootconfig/vbmeta fops 代理、镜像工具与 loader 基于 DSU-Permissive `0.6.1` 提取和修改；
保留原文件 SPDX `GPL-2.0-only`。
