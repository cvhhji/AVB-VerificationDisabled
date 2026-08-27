# AVB-VerificationDisabled

BL-stage ABL AVB 校验关闭工具。对高通 ABL ELF 打补丁，在 bootloader 层面短路 AVB 校验、
强制 `verifiedbootstate=green`，使修改过的 boot/init_boot 镜像能通过 ABL 校验并正常启动。

与 [gbl_root_canoe](https://github.com/cvhhji/gbl_root_canoe) 假回锁配合，实现
**假回锁状态下关闭 AVB 且正常开机，且不被系统发现 BL 已解锁**。

> **高风险实验项目。** patch 成功不等于适配所有 ABL 版本。刷写前必须保留原 abl 分区备份，
> 确保有 fastboot/EDL 救砖路径。

## 原理

对 ABL ELF 应用三类补丁：

| 补丁 | 作用 |
|------|------|
| 强制 verifiedbootstate=green | 将 orange/yellow/red 字符串引用重定向到 green，隐藏解锁状态 |
| 短路 AVB 校验入口 | 校验函数直接返回成功，跳过 boot/init_boot/vendor_boot/dtbo 校验 |
| NOP 错误分支 | 消除跳转到 red/error 状态的条件分支，防止启动失败画面 |

结果：ABL 报告 `device_state=locked` + `verifiedbootstate=green`（与真锁完全一致），
但不执行任何校验，修改过的镜像可正常启动。

## 构建

主机 gcc/clang 即可，不需要 Android NDK：

```bash
make
```

产物：`out/patch_abl_avb`

## 使用

```bash
# 1. 从 abl 分区提取 ABL ELF（使用 gbl_root_canoe 的 extractfv）
extractfv abl.img abl.elf

# 2. 应用假回锁补丁（gbl_root_canoe）
patch_abl abl.elf abl_relocked.elf

# 3. 应用 BL-stage AVB 补丁（本工具）
out/patch_abl_avb abl_relocked.elf abl_final.elf

# 4. 重新打包并刷入 abl 分区
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
- patch 后的 ABL 仍需与 gbl_root_canoe 假回锁 BDS 配合使用。

## 来源与许可

核心 patch 逻辑参考 gbl_root_canoe 的模式匹配方法。GPL-2.0-only。
