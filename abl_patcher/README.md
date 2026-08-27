# BL-Stage ABL AVB Disabler

在 Bootloader (BL) 阶段关闭 AVB 2.0 校验的 ABL 二进制补丁工具，与
[gbl_root_canoe](https://github.com/cvhhji/gbl_root_canoe) 假回锁配合使用，
实现**假回锁状态下关闭 AVB 且正常开机**。

## 背景

### 问题

- gbl_root_canoe 假回锁让 ABL **报告** `device_state=locked`，但 ABL 内部仍执行 AVB 校验。
- init 阶段的 `avb_interceptor.ko` 修改 PID 1 的 vbmeta 读取视图（`flags |= 0x02`），
  让 userspace `libfs_avb` 跳过校验。
- 但如果 vbmeta 出现 `VERIFICATION_DISABLED (0x02)` 标志，标准 AVB 逻辑判定
  `locked + verification_disabled = 安全违规 → 拒绝启动`。
- 同时被修改过的 boot/init_boot 镜像无法通过 ABL 的 hashtree 校验。

### 解决方案

在 BL 阶段对 ABL 二进制打三类补丁。支持 PE/COFF (ARM64 EFI) 和原始 ELF/flat 格式：

| 补丁 | 作用 | 隐蔽性 |
|------|------|--------|
| 短路 AVB 校验入口 | 通过 `AVB0`/`vbmeta` 字符串引用定位校验函数，prologue 替换为 `MOV X0,XZR; RET` | 无外部可见变化 |
| 强制 verifiedbootstate=green | 检测 .data 段中的 boot state 名值数组（`{name*, value}` 16字节步长），将 orange/yellow/red 的名字指针重定向到 green 并修正值；fallback 为 ADRP+ADD 引用重定向 | 系统始终看到 green |
| NOP 错误分支 | 消除跳转到 red/error 状态的条件分支 | 无启动失败画面 |

**结果**：ABL 报告 `locked + green`（与真锁完全一致），但不执行任何校验，
修改过的 boot/init_boot/vendor_boot/dtbo 镜像可正常启动。

## 与假回锁的关系

- **不修改** gbl_root_canoe 的任何补丁（GBL 漏洞利用、unlocked→locked、bootstate 等）。
- 本工具在 gbl_root_canoe 补丁**之后**对 ABL 叠加 AVB 补丁。
- 假回锁负责 `device_state=locked`，本工具负责跳过校验 + 保持 `green`。
- 两者互补，互不冲突。

## 构建

```bash
cd abl_patcher
make
```

依赖：gcc/clang、make。纯主机工具，不需要 Android NDK。

## 使用

### 1. 提取 ABL ELF

使用 gbl_root_canoe 的 `extractfv` 从 `abl.img` 中提取 ABL ELF：

```bash
extractfv abl.img abl.elf
```

### 2. 应用假回锁补丁（gbl_root_canoe）

```bash
patch_abl abl.elf abl_relocked.elf
```

### 3. 应用 BL 阶段 AVB 补丁（本工具）

```bash
patch_abl_avb abl_relocked.elf abl_final.elf
```

也可以直接对未补丁的 ABL 打 AVB 补丁（但没有假回锁时 device_state 仍为 unlocked）：

```bash
patch_abl_avb abl.elf abl_avb_disabled.elf
```

### 4. 重新打包并刷写

将 `abl_final.elf` 重新打包回 `abl.img` 格式，刷入 `abl` 分区。
具体打包方式参考 gbl_root_canoe 的工作流。

## 完整启动链

```
ABL (patched: fake-relock + AVB disabled)
  │
  ├─ device_state = locked      (假回锁)
  ├─ verifiedbootstate = green  (本工具强制)
  ├─ AVB 校验 = 跳过            (本工具短路)
  │
  └─ 加载修改过的 init_boot (含 avbinit + avb_interceptor.ko)
       │
       └─ avbinit 加载 KO → 修改 PID 1 vbmeta 读取视图 (flags|=0x02)
            → userspace libfs_avb 跳过 system/vendor 校验
            → 正常开机
```

### 关于 init 阶段模块

BL 阶段补丁只解决 ABL 对 boot/init_boot/vendor_boot/dtbo 的校验。
system/vendor/odm 等分区的校验由 first-stage init 的 `libfs_avb` 执行，
仍需要 init 阶段的 `avb_interceptor.ko` 来跳过。

如果希望**纯 BL 阶段**方案（不使用 init 阶段模块），需要额外修改磁盘上的
vbmeta 分区（`flags |= 0x02`），让 libfs_avb 直接看到 verification disabled。
但这会修改磁盘数据，且 ABL 自身也会读到该标志（需确保短路补丁已生效）。

## 补丁日志示例

```
=== AVB BL-Stage Patcher ===
Buffer size: 1234567 bytes (0x12D687)
Load base: 0x0

AVB0 occurrences: 3
Boot-state strings found: 2

[short_circuit] 'AVB0' at offset 0x1A2B3C
  -> function start at 0x1A0000 (ref at 0x1A2B40)
[short_circuit] patched function at 0x1A0000: A9BF7BFD A9005FF6 -> MOV X0,XZR ; RET

[force_green] 'green' at file offset 0x2C3D4E
[force_green] 'orange' at 0x2C3D58, 1 ADRL reference(s)
  -> repointed ADRL at 0x1A0120 to green
[force_green] 'red' at 0x2C3D68, 2 ADRL reference(s)
  -> repointed ADRL at 0x1A0200 to green

[nop_error] NOP branch at 0x1A01F8 -> 0x1A0200 (red handler at 0x1A0200)

=== Patch Summary ===
  Verification functions short-circuited: 1
  State strings forced to green: 3
  Error branches NOPed: 1
  Total patches: 4
```

## 已测试设备

| 设备 | 产品名 | ABL 格式 | 状态 |
|------|--------|----------|------|
| 小米 15 Pro | PLK110 | PE/COFF ARM64 EFI | 模式匹配验证通过（6补丁） |

## 注意事项

- **高风险操作**：刷写错误的 ABL 可能导致设备无法启动。务必保留原 ABL 备份和救砖路径。
- **模式匹配**：本工具基于已知高通 ABL (EDK2) 代码结构进行模式匹配，不同 ABL 版本
  可能需要调整。如果补丁未生效，请检查日志中的字符串/函数定位结果。
- **不修改磁盘 vbmeta**：本工具只修改 ABL 二进制，不碰 vbmeta 分区。
- **与 gbl_root_canoe 叠加顺序**：先假回锁补丁，再 AVB 补丁，避免字符串引用被
  假回锁的 unlocked→locked 重定向影响。
