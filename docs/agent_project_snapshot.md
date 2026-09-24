# 当前项目快照

## 2026-09-23 Basic Planning 关节导出和轨迹点开关

- 新增“导出关节轨迹”，直接导出当前选中计划的全部关节数据；与 CDF 导出复用 QSaveFile 原子写入。格式匹配 ik_joint_angles.txt：秒、度、六位小数、制表符，导出存储的 IK 角度，不执行回放符号映射。
- 新增默认关闭的“显示轨迹点”；控制器保存会话显示状态，通过原控制点 overlay 的 showPoints 参数控制球形标记，轨迹连线与末端运行轨迹独立。
- 已验证 Debug/Release RobotQtViewer 与回归目标构建，两种配置各 2 项回归通过；2001 行导出完整、角度符号与单位正确、参考表头一致，球标开关经过实际帧缓冲图像比较；Release 主程序启动退出码 0。


## 2026-09-23 Basic Planning 逆解与实际 TCP 对齐

- 任务：修复导入末端轨迹逆解后，关节回放的喷枪顶点偏离目标；保留 Joint1/4/5/6 的既有符号映射。
- 根因：原求解器的固定理想 IRB4600 DH 与旧固定工具矩阵，不等于当前 URDF 安装位姿及已标定喷枪 TCP；原求解器仅验证自身 DH 残差。
- 所有权：ProjectScene 从当前模型、基座、工具创建独立 RobotInstance 快照；ProjectMotionPlanning 通过可选世界坐标 FK 回调求解；MotionPlanning 控制器连接模型快照并转换输入种子的符号约定。快照计算不修改视口机器人。
- 算法：实际 TCP 有限差分 Jacobian、阻尼最小二乘、步长限制和回溯；对输入矩阵的小幅非正交误差做 SO(3) 投影；非法位姿拒绝。
- 兼容：旧 DH API 在未传回调时保留；无新增项目依赖、无项目 schema 变更，旧关节数据需要重新逆解。
- 验证：11111.txt 全部 749 点成功，实际回放位置最大误差由 176.197 mm 降为 0.000994557 mm，RMS 0.000213805 mm，最大姿态误差 0.000045337 度。Debug/Release 主程序编译、IK/喷涂/导入回归各 3 项通过；Release 主程序启动退出码 0。
- 产物：上一级 build/ik-11111-round-trip.csv、ik-11111-round-trip.png、ik-11111-comparison.png；测试入口 RobotQtViewerIkPlaybackSmoke，固定六点样例位于 SprayMeasurementSmokeTest/ik_tcp_poses.txt。


## 2026-09-17 喷涂距离与角度扩展

- 当前工作区为桌面 `RS2026-BusinessSource-1.0.0/RS2026-BusinessSource-1.0.0`，分支 `main`；下方 2026-07 的路径和分支是历史记录。
- 开始时 `ProjectScene.cpp` 有用户已有修改；保留已校准的 ABB4600 枪口坐标及法向，不重做装配标定。
- 不变量：圆锥与测量共用 FK 枪口位姿；距离取沿工具 +Z 的最近正向 STL 交点；角度为法向偏角，正负按工具局部 X 轴右手规则。
- 所有权：ViewerCore 提供非 Qt 几何测量，viewport services 转发，MotionPlanning 控制器持有本次播放采样，Widget 只显示结果和曲线。
- 安全扩展点：已有 `intersectRayTriangle`、AssetManager CPU 网格、`applyJointValuesToRobotRuntime`、QSaveFile。无新第三方依赖，无项目格式变化。
- 核对风险：burnner 是静态机器人链接；不能使用简化碰撞代理代替 STL；播放采样必须在整组关节应用后；无命中使用无效标记而非 0。
- 已验证：上一级 `build` 的 Release 目标 `RobotViewerCore`、`MotionPlanningEditor`、`RobotQtViewer` 编译通过；默认工程隐藏启动退出码 0；新增 spray smoke 覆盖最近交点、角度符号、STL 绕序、FK/目标移动、无命中、播放采样、自动 TXT 导出和双图像素检查并通过。

更新时间：2026-07-20

## 当前基线

- 工作区：`D:/program/src/RS2026_CodexDev`
- 分支：`branch-codex-dev`
- 基线提交：`5f4ca0fda7fed65fc49690b6c0da1860c12fe87a`
- `thirdparty` 与 `data` 子模块处于主仓库记录的提交。
- 当前工作树包含本轮明确执行的 vcpkg 移除、路径修订和历史文档清理。

## 模块现状

- `SMRobotCore`：机器人模型、IO、运动学、运行时、轨迹、碰撞和核心 SDK。
- `SMRobotPlatform`：资产、渲染、场景、相机、项目文档、仿真运行时、传感器和平台 SDK。
- `SMRobotApps`：`RobotViewerCore`、`RobotQtViewer`、QuickStart 和教程。
- `SimWorkbench`：装配、碰撞、运行、运动规划、喷涂、分析和数字孪生工作台。

## 当前架构依据

- GUI/document-view：`docs/architecture/document_view_gui_contract.md`
- Workbench：`docs/architecture/simulation_platform_document_workbench_contract.md`
- SDK：`docs/architecture/modular_simulation_platform_sdk_contract.md`
- DLL component：`docs/architecture/sdk_dll_component_policy.md`

## 构建状态

- 已在 `build/codex_cleanup_check` 使用 CMake 3.31.3 与 Visual Studio 2019 generator 完成 configure/generate。
- configure 使用 `USER_TANGTANG_4090_Windows=ON`，未启用 example、regression 或 tests。
- 本轮尚未执行目标编译和 CTest。
- 当前 Windows 主开发配置依赖 `D:/PreBuild`，使用 `USER_TANGTANG_4090_Windows=ON`。
- 根目录忽略的 `PrebuiltPackages` 早于当前源码，不应作为当前源码的验证产物。

## 当前风险与下一步

- 下一步构建 `RobotQtViewer` 和 `RenderCoreShaderResources`。
- 测试时同时启用 `BuildTests=ON` 与 `BuildRegression=ON`。
- 不复用旧 `PrebuiltPackages` 证明当前源码可构建；SDK 应使用新的 install prefix 生成。
- 仍有效的后续计划见 `plans/README.md`，执行前必须重新核对源码。

## 2026-07-21 Coating Analysis 厚度预测框架实施快照

- 当前分支：`branch-codex-dev`；开始实施前工作树干净。
- 当前任务：执行 `plans/coating_analysis_thickness_prediction_framework_revision_plan.md`，形成模型导入、演示厚度预测、彩色 surface-scalar overlay、厚度色标与 hover 数值探测闭环。
- 领域所有权：`SMRobotSpray/SprayThicknessPrediction` 只生成厚度标量；通用色图进入 `SMRobotPlatform/VisualizationSDK`；`RobotViewerCore` 负责 scene overlay 与精确 probe；`SMRobotWorkbenchPaintingAnalysis` 负责 Qt workbench 编排。
- 当前可复用点：`SceneEntityWorkflowController`、`ViewportReloadWorkflowController`、`RobotQtViewerDocumentViewRegistry`、`RobotQtViewerSelectionModel`、`GeometryDesc::colors`、`ModelNode::setModel(...)`。
- 当前待退役路径：`ProjectRuntimeBuilder` 依据 `burner.stl` 文件名就地修改缓存 `ModelDesc` 的演示着色逻辑，以及 `RobotViewerCore` 对 `SprayThicknessPrediction` 的条件依赖。
- 构建顺序风险：根 `SeqPackages` 当前在 `SMRobotApps` 之后才加入 `SMRobotWorkbenchPaintingAnalysis`；若 `RobotQtViewer` 链接 Painting Analysis 包，需要将该 workbench 包前移到 `SMRobotApps` 之前，但不得改变 Core/Platform 包顺序。
- 预期验证：分别构建 `SprayThicknessPrediction`、`VisualizationSDK`、`RobotViewerCore`、Painting Analysis 实现 target 与 `RobotQtViewer`，运行新增 headless tests，并执行隐藏启动 GUI smoke。

## 2026-07-21 OMPL 阶段 A-E 实施快照

- 当前分支：`branch-codex-dev`；任务目标是执行 `plans/ompl_motion_planning_project_adapter_integration_plan.md` 到阶段 E，形成无 Qt 的 project/collision/OMPL/headless/GLFW 闭环。
- 当前工作树包含另一项 Coating Analysis 实施中的 `RobotViewerCore`、Qt viewport、`VisualizationSDK`、`SprayThicknessPrediction` 和 Painting Analysis 改动；本任务不修改或回退这些文件。
- 主要影响模块：`SMRobotCore/RobotCore`、`SMRobotCore/RobotIO`、`SMRobotPlatform/SimulationRuntime`、`SMRobotMotionPlanning`，以及 motion planning 自己的 regression/feature probe。
- 所有权：`RobotCore` 保存关节限制；`RobotIO` 解析 URDF limit；`SimulationRuntime` 负责批量关节状态及 attachment 更新；`ProjectMotionPlanning` 构建任务私有 runtime/collision snapshot；`MotionPlanningOmpl` 私有封装 OMPL；GLFW probe 只消费正式服务。
- 当前安全扩展点：`RobotJoint` 是普通数据结构；`URDFLoader` 的 limit 解析当前被注释；`ProjectSimulationRuntime` 已有单关节设置、统一 `update()` 和 attachment propagation；`ProjectCollisionRuntime` 已有 detector ID、update/check/result 接口。
- 当前缺口：`MotionPlanningCore` 只有线性插值且公共 request 直接服务旧调用；`SMRobotMotionPlanning` 只有一个 component；`HeadlessProjectCollisionExample` 与 `ProjectGlfwViewer` 目录均缺少 `main.cpp`。
- 目标项目：`config/projects/420-red4600-tool.sys.json`；规划机器人 `Red4600`，静态环境机器人 `ATPPZ350`，工具 `420_tool_attachment`，detector `default_collision`。
- 风险：新增 `RobotJoint` 字段会改变源码级结构布局但不改变虚函数表；必须同步所有 loader 并构建依赖 target。默认场景未必能稳定找到“直线碰撞但可绕行”的构型，若失败应保留 smoke test 并报告，不得削弱 detector。
- 验证顺序：独立 CMake configure；构建 `MotionPlanningCore`、`MotionPlanningOmpl`、`ProjectMotionPlanning`、headless regression 和 `ProjectOmplPlanningViewer`；运行无窗口固定 seed 场景；最后检查可执行依赖不含 Qt。

### 实施完成状态

- 阶段 A-E 已完成，阶段 F 未开始。
- 新增 `MotionPlanningOmpl`、`ProjectMotionPlanning`、`ProjectMotionPlanning-Headless` 和 `ProjectOmplPlanningViewer`；所有规划业务入口位于非 Qt 模块。
- 固定真实项目 fixture 已验证“起终点合法、直线插值碰撞、RRTConnect 可绕行”，并完成最终路径逐段复验、保存/重载和 runtime 回放。
- `build/codex_ompl_stage_e` Release 构建 `ProjectMotionPlanning-Headless`、`ProjectOmplPlanningViewer`、`RobotQtViewer` 通过；CTest 1/1 通过；隐藏 GLFW/OpenGL 运行通过；PE 依赖不含 Qt。
- 下一步若进入阶段 F，应直接复用 `ProjectMotionPlanningService`，Qt 层不得重新构建 OMPL state space 或直接调用 collision backend。

## 2026-09-23 APF 碰撞段预修复

- 分支 main；根仓库和两个相关子模块工作树在任务开始时干净。
- 在 ProjectMotionPlanning 领域服务替换导入轨迹的 OMPL 预修复；Qt workbench 只更新算法文案。通用 OMPL 规划入口保留。
- 不变量：原始关节符号映射、时间戳、后续 QP/CDF 流程保持；两侧安全锚点固定，替换段及完整路径验证通过后才允许优化，不发布碰撞残留路径。
- 采用实际检测器的显式距离查询生成斥力；受限步长、确定性切向脱困和有限迭代，不增加依赖。
- 验证：私有 APF 算法回归、指定 ik_joint_angles.txt 真实网格场景、Release/Debug 规划测试和 RobotQtViewer 构建。离散运动检查不等于连续碰撞证明。

### APF 任务完成状态

- 已实现 APF 安全锚点预修复、0.001 rad 全流程碰撞验收、连续关节解缠输出及真实回放复验，原 QP/CDF 目标与参数保留。
- Release/Debug 各 4 项回归通过；真实输入 749 → 8641 点，修复与导出重新导入后的线性回放均 0 碰撞段。真实文件最终回归采用 1 轮 QP；间距约 0.00675 mm，未达默认 10 mm，按 partial 如实报告。
- 当前用户运行中的 Release EXE 无法覆盖，最新可执行文件另存为同目录 `RobotQtViewer_APFrx64.exe`；Debug 正常构建。
- 细节、哈希及验证命令见 `docs/agent_change_audit.md` 本日章节和同级 build 目录的 `APF-validation-report.md`。

## 2026-09-23 QP 默认单轮调整

- 用户要求当前代码改为一轮 QP；GUI 控件初值、GUI settings、领域 options 和 headless 默认值统一为 1，保留显式轮数设置能力。
- 保留已有 APF、QP 求解器内部迭代和碰撞验收；当前已开始的旧进程任务不会动态改变参数。
- 本轮只调整默认值，不声称解决计算性能；上一轮真实数据单轮完整流程约 1192 秒。

## 2026-09-23 轨迹减点与平滑

- 保留之前未提交的 APF/单轮 QP 工作。本轮算法归属 ProjectMotionPlanning；不调整关节符号、碰撞检测器或场景几何。
- 优化节点间隔独立为 0.04 rad，运动验收仍为最多 0.001 rad；保留原始输入时间点和首尾配置。
- APF 局部段采用有界碰撞验证捷径；QP 前后采用包含接缝的时间加权弯曲代价下降平滑。后平滑不得降低已达到的最小点间距（达到目标后允许保持目标）。
- QP 连续关节统一使用解缠坐标，避免逐点折回正负 pi 引入虚假折角。
- 验证目标：几何与周期关节回归、749 点输入且匹配 GUI 默认参数的单轮全量验证及平滑度对比，Release/Debug 编译和测试。

- 完成：4529 点、0.001 rad 独立回放与再导入均无碰撞段，QP 一轮耗时 870.532 秒，点间距仍未收敛到 10 mm。Release/Debug 各 5 项回归通过。末端曲线对比与限制见同级 build/APF-smoothing-report.md。

## 2026-09-24 多逆解与调试

- 当前相关工作树干净。本轮只新增直接读取末端控制点的多逆解与 Basic Planning 调试入口，保留单逆解和符号映射。
- ProjectMotionPlanning 拥有有限范围多初值搜索、实际 FK 复验、turn 枚举、去重和单解序列组装；Workbench 拥有后台任务与主从表投影，应用仍走 document/runtime 正式入口。
- 数值搜索不保证枚举全部离散根；无机械限位的 continuous 关节采用显式可配置搜索窗口，不把窗口宣称为物理限位。
- 后台单线程独占 FK 快照，支持取消/进度；源项目、轨迹、机器人改变时丢弃过期结果。
- 验证：领域边界/多圈回归、真实 ABB/TCP 多解复验、GUI 单点/选解播放、Release/Debug 构建。

- 完成：多逆解接口、后台任务、可配置搜索窗口、候选主从表、单点应用和选解序列播放均已实现。真实 749 点得到 5986 组有效候选，搜索约 11.45 秒；Release 5 项、Debug 3 项相关回归通过。可执行文件为同级 build/Release/bin/RobotQtViewerrx64.exe。详见本日 audit 和 multi_ik_debug_usage.md。

## 2026-09-24 多解单点应用后结果消失修复

- 保留上一轮未提交的多逆解实现。根因是应用关节角的文档通知触发 ToolSetup::refresh → syncPinnedRobotMountFrames → ViewportPreviewChanged，旧处理无条件清空多解。
- MotionPlanningModuleController 按 preview payload 区分几何变换和显示/焦点状态；只有几何变化使多解失效。复用正式 preview state，不屏蔽消息、不绕过 document mutation。
- 增补跨面板嵌套通知回归，先确认旧实现失败，修复后验证连续应用、候选切换、选解播放及真正基座变换的失效行为。
- 当前用户原 EXE 在运行，Release 修复版另名为同级 build/Release/bin/RobotQtViewer_MultiIKFixrx64.exe。

## 2026-09-24 分层图 Top-M 筛选与 CDF 初始解

- 开始时根仓库及相关子仓工作树干净，基线已包含多逆解和应用后结果保留修复。
- 领域 ProjectMotionPlanning 新增分层图 API：仅相邻层全连接、无节点代价/碰撞检测，以实际未折回关节差和可配置权重精确求 Top-M；回溯存储受显式预算约束，超限报错而非静默裁边。
- Workbench 增加 M/权重、筛选/取消、结果排名与逐点明细，后台计算，源逆解失效时取消旧任务。选中结果以原 IK 符号、度数和时间戳进入现有 CDF 初始栏。
- 本轮不自动执行 APF 或 CDF，不进行碰撞后的最终 Top-K 排名。用户本轮的前 K 组输出按可配置 M 条运动学候选实现。
- 验证：小图穷举对比、turn/同价/无解/取消/预算回归、GUI 到 CDF 符号时间对照、真实 749 点、Release/Debug 构建测试。

- 完成：精确 Top-M API、后台筛选/取消、结果及完整明细、选中候选进入 CDF。Release 4 项/Debug 2 项回归通过；真实 749 点 Top-30 图计算约 0.0095 秒。当前可执行文件恢复为同级 build/Release/bin/RobotQtViewerrx64.exe。

## 2026-09-24 构型选择对比窗口

- 保留当前未提交的 Top-M 实现。新增 Basic Planning 按钮及 Qt 绘图窗口，由 controller 从完整排名结果投影一基逆解编号，不读采样表格，不改变筛选或优化算法。
- 支持任意多选、叠加/分行和控制点区间；源结果失效时关闭旧窗口。编号仅代表当前点内的候选。
- 验证完整序列投影、多选/区间、生命周期、Release/Debug 构建和 GUI 回归。

- 完成：对比窗口及全量只读投影已接入，Release 三项/Debug 一项 GUI 回归通过，749 点单点差异绘图已视觉核对，主程序启动检查退出 0。可执行文件为同级 build/Release/bin/RobotQtViewerrx64.exe。
