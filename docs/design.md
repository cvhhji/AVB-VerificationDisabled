# 设计摘要

## Init 阶段拦截器

拦截窗口从 KO 在原始 init 前加载开始，到 PID 1 exec `/system/bin/init` 为止。kprobe pre-handler
只识别 PID、阶段和 file/device；实际代理通过复制 file_operations 完成。bootconfig 代理维护逻辑
偏移，将 orange 键置于原内容之前。vbmeta 代理先调用原始 read/read_iter，再验证 `AVB0` magic，
只对返回缓冲区绝对偏移 123 OR 0x02。模块引用跟随代理 fops owner，由 release 回收。

该实现故意不伪造 second-stage 属性，不写块设备，也不保持长期 Hook。失败时默认透传原读取结果；
若目标设备路径未及时解析或 first-stage 使用不同读取/状态来源，则功能不生效。

### Green 模式（假回锁兼容）

当模块参数 `avb_keep_green=1` 时（由 avbinit 检测 ramdisk 中的 `/avb_keep_green` 自动设置），
bootconfig 代理跳过 orange 注入，让 ABL 提供的 `verifiedbootstate=green` 原样传递。
vbmeta flags 补丁不受影响，`libfs_avb` 仍因 `VerificationDisabled` 跳过校验。
此模式配合 gbl_root_canoe 假回锁 + BL 阶段 ABL 补丁使用，使系统看到完全的 `locked + green` 状态。

## BL 阶段 ABL 补丁

`abl_patcher/patch_abl_avb` 对高通 ABL ELF 进行二进制模式匹配和指令补丁，实现 bootloader
层面的 AVB 校验关闭。

### 补丁策略

1. **短路校验入口**：通过 `AVB0` magic、`vbmeta`、`VerifiedBoot` 等字符串引用定位校验函数，
   将函数 prologue 替换为 `MOV X0, XZR; RET`（返回 EFI_SUCCESS）。

2. **强制 green 状态**：搜索 `orange`/`yellow`/`red` UTF-16LE 字符串，找到加载它们的
   ADRP+ADD 指令对，重写为指向 `green` 字符串。确保 ABL 始终报告 green。

3. **NOP 错误分支**：定位跳转到 red/error 处理代码的条件分支（CBZ/CBNZ/B.cond），
   替换为 NOP，防止进入启动失败状态。

### 与假回锁的关系

- 不修改 gbl_root_canoe 的任何补丁（GBL 利用、unlocked→locked、bootstate 等）。
- 在假回锁补丁之后叠加应用，两者互补不冲突。
- 假回锁负责 `device_state=locked`，BL 补丁负责跳过校验 + 保持 `green`。

### 边界

- 基于已知高通 ABL (EDK2) 代码结构的模式匹配，不同 ABL 版本可能需要调整。
- 只修改 ABL 二进制，不碰 vbmeta 分区或其他磁盘数据。
- 短路整个校验函数可能影响 ABL 的启动参数填充，需在真机上验证。
