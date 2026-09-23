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
