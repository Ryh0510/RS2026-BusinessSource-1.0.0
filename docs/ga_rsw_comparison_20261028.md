# 当前 A 与 RSW 遗传算法及 1.txt 差异定位（2026-10-28）

## 结论及证据边界

当前 A 默认场景与 RSW 已验证跑通 1.txt 的保存场景使用不同的工具 TCP 姿态，这是本次已用单变量对照确认的主要原因。另有喷射轴旋转开关、完整初始池的触发条件、进化顺序和后续喷距约束差异。

“RSW 跑通”的可核实证据来自 `RSW/verification/full-13-solution/full13-ga-final-no-densify/` 和同目录报告：原始文件 SHA 与用户桌面 1.txt 一致，100 mm、45° 法向半角、允许轴转，GA/APF/CDF 和最终复核通过，输出50点。该记录实际场景是 `RSW/verification/endpoint-optimization/scene.sys.json`。RSW 当前默认 `config/projects/ABB4600-Aburnner.sys.json` 的 TCP yaw 本身也约为0°，不能将成功保存场景和默认场景混为一谈。未读取用户另一运行实例的实时项目，本文不推断该实例当前加载了哪份场景。

## 按影响顺序比较

| 顺序 | 项目 | 当前 A 按钮流程 | RSW 源码/已成功任务 | 影响 |
|---|---|---|---|---|
| 1 | 实际工具 TCP 定义 | assetMountToFrame.yaw=8.881784197001252e-16，约0° | 跑通记录的场景为3.141592653589792，约180° | 同一目标 TCP 被换算为不同法兰姿态；实际枪体朝向、IK与挡板/工件碰撞不同。已实测确认。 |
| 2 | 绕喷射轴旋转 | 领域请求默认false，按钮未打开；本轮候选保持最短摆动，无额外自由轴转 | Agent明确传true；原生兼容默认也为false | RSW搜索多一个轴向自由度，法向半角不消耗轴转量。A没有完整复现Agent条件。 |
| 3 | 已安全控制点 | gaEvaluateAllPoints=true，已安全点也完整跑5000初始池和8轮 | 安全点可保留原解，只有进入修复的点跑完整池 | A更耗时。不是末点无安全候选的直接原因。 |
| 4 | 初始池及基础GA | 5000、8轮、24活动种群、131072评估预算 | 当前RSW相同；PointSearchPopulation、SurfacePostureSearch、SurfaceFeasibilitySampling代码相同 | 不能用初始候选数不足解释当前失败。5000不是活动种群大小。 |
| 5 | 本轮进化扩展 | 有安全父代即在基本24后代中采用安全父代交叉/变异；开放半径后每轮无父代时先做256批量球壳搜索 | 保留原四维随机序列和每轮24固定距离后代；半径开放后另加24变距后代；8轮后的补救才批量覆盖并用安全父代细化 | A本轮按用户新增要求改过搜索顺序，随机轨迹和父代分配不再完全相同。 |
| 6 | 半径计划 | 第3/4/5轮无安全姿态时开放±30/50/80%，找到后冻结 | 当前RSW相同；GaRadiusEvolution仅换行不同 | 基本半径规则一致。 |
| 7 | APF/CDF喷距 | 将每点实际开放的最小/最大半径范围传到后续阶段 | Agent提取GA最终选中的逐点距离，后续固定该距离及有序插值 | 后续轨迹问题并不完全相同；本次1.txt停在GA，尚不能用此解释本次失败。 |
| 8 | 调度/连续种子 | 领域服务直接GA→APF→回放→可选CDF→回放；使用导入关节值作首选解，默认1条候选路径 | Agent先全IK/原始点检查→GA→原始点独立复核→可选加密→APF→全路径复核→可选CDF；传递cached/preferred IK，continuous_path_count=3 | 缓存、候选选择和连接条件影响耗时与后续可连接性。 |
| 9 | 时间与历史版本 | 工作台取消单点超时，仍有131072评估预算 | 当前Agent缺省单点900s、阶段3600s；旧13点成功记录仅96次配置预算和8s单点、45° | 旧成功记录属于旧搜索版本，不能用其约107s总耗时估算当前5000完整池流程。当前A失败也不能再归因于180s超时。 |

## 单变量实测

用当前 A 重新构建的 `C:/b/rs106-20261028/Release/bin/RwsTrajectoryEvaluate.exe`，相同输入、相同模型/检测器和真实CPU安全检查执行：

| 场景 | 原始13点拥有安全候选的点数 | 备注 |
|---|---:|---|
| A当前默认TCP，yaw≈0° | 1/13 | 被拒绝IK根统计：实体碰撞144，虚拟挡板32 |
| A测试副本，只将TCP yaw改为≈180° | 7/13 | 实体碰撞32，虚拟挡板64 |
| RSW原成功保存场景 | 7/13 | 拒绝统计与上一行相同 |

安全点数并不表示完整路径通过；三者原始13点均未全部安全，故独立原始点请求整体success=false属于预期。

再以完全相同的RSW已接受50点最终精确关节轨迹，交给当前A程序重新复核，设置80°、轴转false：

- A yaw≈0°：拒绝，50个源层都报 `spray_target_error`，第0点即与目标/球心映射不匹配。
- A测试副本 yaw≈180°：50点全部有效，整条路径采样复核success=true，`sampled_path_valid`；没有重新求解替代轨迹，也没有关闭碰撞/挡板。

这说明当前程序能够验收RSW的已知解，主要差异确实来自TCP定义及请求条件；不等于已经在正式A场景中从原始1.txt重新完成新GA搜索。对照副本只在证据目录内，正式场景保留原值。

## 已排除及未证明事项

A与RSW的A.STL、A2(1).STL、FANUC URDF逐字节SHA一致。机器人基座、工件位姿、工具安装位姿、检测器定义、虚拟挡板开关和用户坐标系一致；用户坐标系为单位变换，故这次world/user标签差异不改变数值。虚拟挡板、GPU串联IK及GPU挡板搜索实现一致。VirtualBaffleEnvironment、GpuSerialIk、GpuBaffleSearch、PointSearchPopulation、SurfacePostureSearch、SurfaceFeasibilitySampling及GaRadiusEvolution忽略换行后相同。

没有证据表明RSW成功依赖关闭碰撞或挡板。当前A的1.txt在100mm、末点21976次/约679秒仍无安全姿态，验证进程主动停止，不能宣称已穷尽131072预算或证明无解。

## 建议修复顺序

1. 先以本次测试副本确认1.txt应使用的工具TCP定义，并让正式工作台使用与成功任务一致的定义；避免通过增大GA预算弥补不同的TCP输入条件。
2. 将“法向80°”与“允许喷射轴旋转”作为两个独立条件，按RSW任务条件传递；保留真实碰撞/挡板门禁。
3. 再决定是否严格复现RSW的原始IK缓存、仅修复不安全点、原四维24后代序列，以及GA选定喷距后固定传递给APF的方式。
4. 在相同场景、同一TCP、同一喷距/轴转/时间策略下，从原始1.txt做新旧GA对照，分别报告单点、连接和整条路径状态。

## 证据与定位

证据目录：外层 `output/ga-surface-20261028/rsw-comparison/`。

- `controlled-comparison-summary.json`：上述5次对照汇总。
- `same-original-ik-request.json`：三场景共用原始输入。
- `same-rsw-final-path-request.json`：两场景共用精确50点路径。
- `A-absolute.sys.json` / `A-test-tcp-yaw180.sys.json`：测试副本，只有后者改变TCP yaw。
- `*-ik.json`、`*-rsw-final-path.json`：实际原生返回。
- 代码：A `GaApfCdfPipeline.cpp:109`、`ExternalTrajectoryEvaluation.h:82`、`ExternalTrajectoryEvaluation.cpp:1212/1341/1360`；RSW Agent `ai/rws_pipeline.py:121/142`、`ai/rws_segmented.py:45`；RSW原生 `ExternalTrajectoryEvaluation.cpp:1203/1345/1370`。

未修改RSW的源码、历史场景或结果，也没有修改用户1.txt。此报告分析和对照不自动覆盖正式TCP配置。
