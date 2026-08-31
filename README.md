# AVB-VerificationDisabled

高通设备 EFISP Android loader 工具包。目前默认生成安全基线：从原始 `abl.img`
提取 `LinuxLoader.efi`，只应用 gbl_root_canoe 的假回锁补丁，并输出
`efisp/boot.efi` 与 `efisp/BOOTENTRIES`。

与 [gbl_root_canoe](https://github.com/cvhhji/gbl_root_canoe) 假回锁配合，实现
在真实 bootloader unlocked 状态下保留原有 AVB 行为，并伪装系统可见的锁定状态。

> **高风险实验项目。** EFI 生成成功不等于设备实机验证成功。刷写前必须保留原 efisp/abl 备份，
> 确保有 fastboot/EDL 救砖路径。

## 原理

仓库中的实验性 `patch_abl_avb` 曾按 `AVB0`、`vbmeta` 和
`androidboot.vbmeta` 字符串引用猜测验证函数。真实样本中每份会命中 7–9 个
不同函数，其中包含解析器和启动参数生成逻辑；覆盖这些函数会破坏启动。

因此默认工具包不会运行该实验补丁。`boot.efi` 基于原始 LinuxLoader，AVB
是否放行服从设备真实解锁状态；假回锁补丁只处理系统可见状态和警告。

## 构建

主机 gcc/clang 即可，不需要 Android NDK：

```bash
make
```

Actions 只发布 Linux、Android 和 Windows 三个平台工具包。把自己设备当前
版本的 `abl.img` 放入工具包后在本地生成 `efisp/boot.efi`。

## 使用

```bash
# 将当前设备的 abl 分区镜像放入工具包
cp abl.img images/abl.img
./build.sh

# 把生成的 efisp 目录部署到 persist 中 gbl_root_canoe 使用的位置；
# 不要把 boot.efi 直接刷进 abl 分区。
```

可选参数 `--load-base 0xADDR`：指定文件偏移 0 对应的运行时地址（从 FV 提取的 ABL 通常为 0）。

## 测试

将 ABL ELF 样本放入 `tests/samples/`（支持 `.elf` 和 `.bin`），然后：

```bash
make test
```

测试脚本会对每个样本运行 patcher，验证输出文件存在且非空。
无样本时自动跳过 patch 测试，仅执行基本功能测试。

## 明确边界

- 仅支持高通 ABL（UEFI 应用），不支持其他厂商 bootloader。
- 依赖 ABL 中存在特定的 boot-state 字符串和 AVB 校验函数模式；不同版本/厂商可能需要调整匹配模式。
- 不修改 vbmeta 分区，不签名镜像，不修改 KeyMint/TEE 证明结果。
- `boot.efi` 只能放进 gbl_root_canoe 的 EFISP 文件目录，不能直接刷入 abl。
- 实验性 AVB 短路器保留用于诊断，但遇到多候选会拒绝生成输出。

## 来源与许可

核心 patch 逻辑参考 gbl_root_canoe 的模式匹配方法。GPL-2.0-only。
