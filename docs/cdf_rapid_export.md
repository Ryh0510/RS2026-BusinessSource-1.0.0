# CDF RAPID 模块导出

在 Motion Planning → CDF 中，选择优化得到的最终轨迹，点击“导出 CDF RAPID 程序（.mod）...”，选择保存位置。按钮位于原有 TXT 导出按钮下方；无最终关节轨迹时或播放期间禁用。

导出来自完整计划的所有点，不是表格抽样行。按现有 ABB 关节符号映射调用当前实际模型的独立 FK 快照，保留多圈值，计算喷枪 TCP；不运行旧理想 DH、不移动主窗口机器人、不重算 APF/QP。

格式沿用附件的 MODULE MainModule、CONST robtarget p1…pN、PROC main、MoveL：

- 位置：当前场景世界坐标，单位 mm，对应 RAPID wobj0。若控制器世界坐标与场景不同，必须配置对应工件坐标变换。
- 姿态：归一化四元数 [w,x,y,z]，相邻四元数保持同半球；位置6位小数，四元数9位小数。
- 工具：按用户最新指定的桌面 MainModule.mod 固定为 tool0；MoveL 使用 tool0\WObj:=wobj0，不输出自定义 tooldata。保留当前 TCP 的目标坐标数字，不自动转换成补偿工具偏置的法兰轨迹；tool0 将这些坐标解释为法兰目标。仍核对 TCP 相对法兰固定，防止错误的 FK 快照。
- 参数：与附件一致使用 v50、z10、无外轴 9E+09。
- 编码：ASCII（UTF-8无BOM兼容）、CRLF；通过QSaveFile原子保存，失败不留下半份程序。

项目当前没有可验证的 ABB confdata 映射。robconf 按附件保留 [0,0,0,0] 占位；使用控制器内置 tool0，不另声明负载；没有把 B1～B8 当作 ABB 构型编号。此文件可用于轨迹点交换及 RobotStudio 后处理，不是可直接保证复现 Top-K 关节分支的已调试控制器程序。

MoveL 与 z10 的线性插补/转角过渡不同于 CDF 验证的关节插值，v50 也不复现原时间戳。实际执行前须完成构型、负载、坐标标定及控制器插补路径验证。本轮未使用 ABB RobotStudio 编译器或真实机器人验证。

实现归 ProjectMotionPlanning/RapidTrajectoryExport；Workbench 提供最终计划与实际 TCP FK 快照并负责保存对话框。新增 API，不改变现有 TXT、项目文件格式、预编译 SDK 或第三方依赖。
