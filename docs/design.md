# 设计摘要

拦截窗口从 KO 在原始 init 前加载开始，到 PID 1 exec `/system/bin/init` 为止。kprobe pre-handler
只识别 PID、阶段和 file/device；实际代理通过复制 file_operations 完成。bootconfig 代理维护逻辑
偏移，将 orange 键置于原内容之前。vbmeta 代理先调用原始 read/read_iter，再验证 `AVB0` magic，
只对返回缓冲区绝对偏移 123 OR 0x02。模块引用跟随代理 fops owner，由 release 回收。

该实现故意不伪造 second-stage 属性，不写块设备，也不保持长期 Hook。失败时默认透传原读取结果；
若目标设备路径未及时解析或 first-stage 使用不同读取/状态来源，则功能不生效。
