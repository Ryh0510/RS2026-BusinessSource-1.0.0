# CDF 最终轨迹导出 FANUC LS

在 Motion Planning → CDF 选择最终优化轨迹，点击“导出发那科机器人程序（.LS）...”，选择保存位置。默认文件名 JHA.LS；文件名主体须为 1–8 位英文字母、数字或下划线，以字母开头，程序名自动转为大写。无最终结果或正在播放时按钮禁用。

导出格式取自用户桌面 JHA.LS，不依赖该文件在运行机器上存在：

- `/PROG`、`/ATTR`、`/MN`、`/POS`、`/END`；`LINE_COUNT` 等于轨迹点数加两条坐标系指令。
- `UFRAME_NUM=2`、`UTOOL_NUM=1`；每点 `L P[n] 400mm/sec CNT100`。
- 每个 `P[n]` 为 `GP1`、`UF:2`、`UT:1`、`CONFIG:'N U T, 0, 0, 0'`、XYZ 和 W/P/R。
- 位置 mm、姿态 deg，均保留三位小数；ASCII、无 BOM、CRLF，原子保存。

数据来自完整 CDF 最终关节轨迹，非 UI 表格抽样，也非 APF 中间阶段。使用实际机器人及喷枪 TCP 的独立 FK 快照；保留多圈关节值，按既有规则还原 ABB 存储关节符号，不移动主窗口机器人，不重跑优化。W/P/R 使用 `Rz(R) Ry(P) Rx(W)`，处理正负 90° 奇异姿态并在等价欧拉角中优先选择邻近表达。

当前项目没有经验证的 FANUC 机器人构型和控制器标定映射，因此本功能按模板交换末端目标位姿：

1. 输出当前喷枪 TCP 的场景世界坐标，并在 LS 中标为 UF2/UT1。控制器 UF2 应与场景世界坐标对应，UT1 应与所用 TCP 对应；不会仅通过填入编号就自动标定或转换坐标系。
2. `N U T, 0, 0, 0` 沿用附件的模板值，不是从 ABB 的肩/肘/腕分类或 turn 推导的 FANUC 构型。需要在目标 FANUC 模型上核对各点可达性、构型与圈数。
3. `L/CNT100` 的笛卡尔直线及圆滑插补不同于 CDF 的关节插值，固定 400 mm/s 也不保留 CDF 时间参数化。因此原 CDF 碰撞与速度验证不能直接证明该控制器程序可安全执行；需要在目标控制器或仿真环境复验。

导出模块属于 `ProjectMotionPlanning/FanucTrajectoryExport`；Workbench 只连接按钮、提供快照和保存路径。没有改变既有 TXT/MOD 导出、APF/CDF 算法、项目持久化结构或预编译 SDK，也没有新增第三方依赖。

验证包含领域序列化/姿态重建测试，以及原 MainWindow 的 `--profile-fanuc-export` 诊断：实际点击 CDF 按钮保存文件，将全部导出位姿与原轨迹实际 FK 对比。不使用简化测试窗口代替主界面；未进行 FANUC 控制器编译或实机验证。

验收结果（2026-10-12）：Release/Debug 主程序构建及各 3/3 回归通过；正常启动原主窗口 CDF 页导出 4528 点，两配置均全部通过实际 FK 核对，最大位置误差约 0.000835 mm、姿态误差约 0.001037°。三位小数导出未丢点。两个发布入口为 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 和外层 build/Release/bin/RobotQtViewerrx64.exe。
