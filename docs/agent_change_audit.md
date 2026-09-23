# 当前变更审计

## 2026-09-23 Basic Planning 新增导出与球形标记显示控件

- Widget 新增“导出关节轨迹”按钮和默认未勾选的“显示轨迹点”；无关节结果时导出按钮禁用。
- 控制器把 Basic Planning 与 CDF 的 TXT 导出合并到同一实现，保留 CDF 专属校验；写出完整源数据而非表格预览，预先验证全部行及有限值，使用 C locale、六位小数和 QSaveFile 原子保存。
- 参考格式为 time_s 与 J1_deg..J6_deg；只把逆解结果的弧度转换为度，不附加运行时符号转换。
- ViewerCore 的控制点 overlay 新增可选 showPoints 参数，控制球形标记；现有两参数调用仍按原行为显示球标。Basic Planning 显式传入勾选状态，刷新后保持状态，原始轨迹连线和末端轨迹线不受该开关影响。
- 验证 Debug/Release 编译与各 2 项回归通过，覆盖 2001 行完整导出、全部时间和角度数值、符号、六位精度、参考表头、默认隐藏、刷新保持以及实际渲染关闭/恢复对比；Release 启动检查退出码 0。
- 无新增依赖，无项目存储格式变化；保留此前逆解修复及所有既有未提交修改。


## 2026-09-23 逆解回放 TCP 偏差修复

- 保留现有关节取反规则和喷枪标定数据。将 Basic Planning 的求解目标绑定到当前实际模型及与圆锥相同的 TCP，避免理想 DH 与 URDF 混用。
- ProjectScene 的 FK 快照自持模型和 RobotInstance，支持当前基座与挂载工具坐标；迭代不会移动真实视口实例。视口服务仅转发，不增加模块依赖。
- CartesianIkOptions 新增可选 worldForwardKinematics 回调；独立领域算法使用实际 FK 数值 Jacobian、阻尼与回溯。未提供回调的旧调用保留原 DH 行为。导入矩阵求解时正交化，输入文件和存储的原始轨迹不改写。
- 控制器把当前运行时种子转换为既有 IK 符号约定，求解结果仍以原约定存储和应用；Basic Planning 按钮信号到存储再到实际应用已回归验证。
- 新增六点固定回归和整文件 CSV 验证入口，覆盖实际 TCP 误差、符号保持、求解不移动视口、旋转 180 度、小幅非正交输入及无效输入；继续通过原喷涂与导入回归。
- 749 点实测：旧算法全部报告成功但最大实际位置误差 176.197 mm；修复后最大 0.000994557 mm，RMS 0.000213805 mm，最大姿态误差 0.000045337 度。
- Debug/Release 主程序及相关测试目标编译通过；两种配置各 3 项相关回归通过，Release 启动退出码 0。Debug 链接存在既有 OMPL 缺失 PDB 的 LNK4099 警告，不影响运行验证。
- 不自动改写已保存的旧关节轨迹，用户应重新执行逆解。项目 C++ 依赖和存储格式不变；对比图绘图依赖仅安装在上一级 build/ik-plot-deps。


## 2026-09-17 Motion Planning 喷涂距离与角度

- ViewerCore 将 ABB4600 一体喷枪/挂载工具统一解析为 TCP，圆锥和测量共用该 FK 位姿；圆锥调整为长 0.11 m、半径 0.04 m、64 段。
- 喷涂距离取工具 +Z 射线与 `burnner` 原始 visual STL 的最近正向交点，CPU 网格按工程生命周期缓存；不依赖碰撞代理或碰撞显示状态。
- 喷涂角度为表面法向偏角：垂直 0 度、掠射 90 度；STL 法向先朝枪口统一，正负由 `(normal cross ray) dot localX` 确定。
- 单点轨迹/CDF 应用和动态播放均刷新测量；播放按每组关节角记录，支持提前预约或结束后直接导出 TXT，并用独立窗口上下绘制距离/角度曲线。
- 无交点不写 0，而是显示无效原因、TXT 写 `nan` 和 `valid=0`、曲线断线。
- 无新增第三方依赖或项目 schema。Release `RobotViewerCore`、`MotionPlanningEditor`、`RobotQtViewer` 和 Debug `RobotQtViewer` 构建通过；默认 `ABB4600-burnner` 工程隐藏启动退出码 0；`RobotQtViewerSprayMeasurementSmoke` 1/1 通过。

更新时间：2026-07-20

## 变更目的

删除未完成且不可用的 vcpkg 接入，修复工作区更名后的旧路径，并清理会误导后续开发的历史计划、阶段快照和完成记录。

## 变更分组

- 构建入口：删除顶层 vcpkg 开关和硬编码 toolchain；删除 `vcpkg.json` 与 vcpkg 专用用户配置。
- 路径修订：项目配置改用仓库相对资产路径；保留文档中的旧工作区改为 `RS2026_CodexDev`。
- 文档清理：删除 153 份历史计划和 70 份历史 docs；保留 10 份未来计划及 36 份架构、SDK、参考和当前设计文档。
- 文档入口：重写 `README.md`、当前快照和本审计；新增 `plans/README.md`。

## 行为与兼容性

- 当前 `USER_TANGTANG_4090_Windows` + `D:/PreBuild` 构建路径不变。
- `tri_robot_collision.scene.json` 的 UR10 路径改为仓库相对路径，换目录后仍可解析。
- vcpkg 构建入口不再提供；以后若需要，应重新设计 toolchain、manifest、preset 和 CI 验证闭环。
- 删除内容仅为文档，可从 Git 历史恢复；未删除源码、公共 API、项目 schema 或模块 target。

## 验证范围

- 构建配置和保留文档不再包含旧工作区路径、旧 vcpkg 开关或活动 vcpkg 配置。
- 保留文档不存在指向已删除 Markdown 文件的引用。
- 所有 `config/projects/*.json` 解析通过。
- `scripts/build_sdk.ps1` 与 `scripts/package_business_source.ps1` 通过 PowerShell AST 语法检查。
- `git diff --check` 通过，修改文件行尾统一为 CRLF。
- `build/codex_cleanup_check` 使用 Visual Studio 2019 完成 CMake configure/generate；仅有既有 PCL/CMake policy developer warning。

## 第一阶段 P0 交付闭环审计

### 变更目的

完成 `RobotQtViewer` Runtime Bundle、Core/Platform Debug+Release PreBuild、无 Core/Platform 源码的业务开发包以及统一一键发布入口。

### 主要变更

- `SimulationProject` 新增 `RuntimePaths`，统一 app/config/data 根目录解析；Qt、ViewerCore、SimulationRuntime 和 ProjectSession 改用该入口。
- 新增 `scripts/package_app_release.ps1` 和 `scripts/release_phase1.ps1`。
- `scripts/build_sdk.ps1` 默认双配置和 standalone profile；SDK profile 补齐 `RobotPlatformAuthorization`。
- `scripts/package_business_source.ps1` 新增 license 白名单、敏感材料扫描、双配置验证、可选用户配置和短验证构建根。
- 修复 PreBuild 模式下 `RobotQtViewer` 缺失 SimulationRuntime/Sensor 依赖声明的问题。
- ABI 快照格式升级为 v2，保留 v1 历史；修复“关闭基线要求仍隐式比较默认基线”的矩阵逻辑。

### 验证证据

- `RobotQtViewer` 与 `SimProject-V3IoSmokeTest` Release 构建成功；运行路径回归 1/1 通过。
- Runtime Bundle 隐藏启动冒烟通过；包内 1 个主程序、40 个顶层运行 DLL、Qt `platforms/qwindows.dll`、唯一指定 `.LIC` 和 runtime manifest 均存在。
- SDK 独立消费矩阵 Debug 21/21、Release 21/21 通过；ABI v2 强制比较通过。
- 业务包内无 `SMRobotCore`、`SMRobotPlatform` 和 staging build；`RobotQtViewer` Debug/Release 均从复制后的 PreBuild 成功构建。
- 统一入口 `scripts/release_phase1.ps1` 全流程成功，生成三类 ZIP、顶层 `release-manifest.json`、`SHA256SUMS.txt` 和 `verification-report.txt`。

### 已知限制

- 当前 Windows 用户配置仍依赖团队既有 `D:/PreBuild` 中的 Qt、Boost、PCL 等完整开发环境；standalone PreBuild 只承诺 Core/Platform SDK 自身可独立消费。
- PCL 在 CMake 3.31 下仍产生既有 CMP0144/CMP0167 developer warning，不影响本次构建。
- 正式发布必须从干净提交执行；本次本地验收使用 `-AllowDirtyTree`，产物仅作验证，不作为正式对外版本。
- 本轮未执行目标编译和 CTest。
# 2026-07-20 Runtime data 与发布策略补充修订

## 修改目的

- 保持项目文件 `data/...` 相对路径，同时让发布程序优先加载可执行程序旁或显式指定的 data 目录。
- Runtime 默认不携带 data，仅在显式参数下复制。
- 统一发布默认复用现有 Core/Platform PreBuild，仅在 `-IncludeSdk` 时重编并单独导出 SDK。
- 首次发布默认版本调整为 `1.0.0`，保留 `-Version` 自定义能力。

## 修改文件组

- 路径解析：`SMRobotPlatform/SimulationProject/src/AssetResolver.cpp`。
- 回归测试：`SMRobotPlatform/SimulationProject/regression/ProjectV3IoSmokeTest/main.cpp`。
- 发布脚本：`scripts/release_phase1.ps1`、`scripts/package_app_release.ps1`、`scripts/package_business_source.ps1`、`scripts/build_sdk.ps1`。
- 版本与说明：`CMakeLists.txt`、`README.md`、`plans/phase1_p0_runtime_release_policy_revision_plan.md`、`plans/README.md`。
- 保留用户已有的 `cmake/ProjectPackagesConfigSetting.cmake` 修改，没有覆盖或归并其所有权。

## 行为变化

- `data/foo` 首先解析为 `<dataRoot>/foo`；仍兼容旧的 `<legacyRoot>/data/foo`。
- `SMROBOT_DATA_ROOT` 的正式契约是直接指向 data 目录。
- `release_phase1.ps1` 默认要求已有 `build/phase1_sdk_install`，不再默认执行 SDK 双配置构建和消费矩阵；`-IncludeSdk` 恢复完整 SDK 构建与独立归档。
- 顶层发布清单只记录本次明确生成的 artifact，旧 ZIP 不会混入。
- Runtime 默认打包显式排除构建输出中可能残留的 `bin/data`，只有 `-IncludeRuntimeData` 才从指定 `DataRoot` 复制模型数据。
- 项目和发布脚本默认版本为 `1.0.0`。

## 验证结果

- PowerShell AST：通过。
- CMake configure：通过，项目版本显示 `1.0.0`。
- `SimProject-V3IoSmokeTest` Release：通过。
- `RobotQtViewer`、`RenderCoreShaderResources` Release：通过。
- Runtime 真实项目 profile：通过，资产日志命中可执行程序旁 `data`。
- SDK Debug/Release 增量构建、install、ABI、consumer matrix：通过。
- 默认统一发布：通过，只生成 Business Source 与 Runtime 两个 ZIP，SDK ZIP 不存在。
- 默认 Runtime ZIP 内容复验：`dataEntryCount=0`，manifest 为 `dataMode=external`、`dataRootContract=data-directory`，启动验证通过。

## 已知限制

- Runtime 默认不包含模型库；使用者需要提供外部 data 目录或显式使用 `-IncludeRuntimeData`。
- Business Source 仍内嵌 PreBuild，因此复用 prefix 前应确保它来自需要交付的 Core/Platform 版本；修改 Core/Platform 后应至少执行一次 `-IncludeSdk` 或独立 `build_sdk.ps1` 刷新 prefix。
- VS2019 对过长构建路径仍可能出现 tracking-log 目录错误，验证构建应继续使用较短的 build 目录。

# 2026-07-21 Coating Analysis 厚度预测框架实施审计

## 变更目的

- 在 `RobotQtViewer` 的 `Coating Analysis` 工作台形成“导入模型、占位厚度预测、彩色显示、色标、悬停厚度”的第一阶段闭环。
- 保持项目文档、喷涂领域算法、通用可视化、Viewer 运行时和 Qt 工作台之间的所有权边界。

## 主要变更

- `SMRobotSpray::SprayThicknessPrediction` 新增确定性的 `DemoThicknessPredictor`，输出单位为米的 `ThicknessPredictionResult`。
- 原 `ThicknessVisualization::evaluateDemoBurnerThickness(...)` 保留为兼容入口并转发到 `DemoThicknessPredictor`；旧色彩入口仅为 SDK 源码兼容保留，新的权威色图实现位于 `VisualizationSDK`，禁止新增调用者。
- `SMRobotPlatform::VisualizationSDK` 新增通用 `ScalarColorMap` 和 surface-scalar 数据契约，不依赖 Qt、OpenGL 或 spray 类型。
- `RobotViewerCore` 新增 overlay apply/show/clear、独立彩色 render model、原模型恢复，以及射线三角形命中和重心插值 probe。
- 删除 `ProjectRuntimeBuilder` 中由 `burner.stl` 文件名触发的自动厚度着色及 ViewerCore 对喷涂厚度模块的直接依赖。
- `SMRobotWorkbenchPaintingAnalysis` 新增分析会话、网格到 workpiece sample 的显式 binding、面板、垂直 legend、导入/预测控制器和对话框服务。
- `RobotQtModulesShared` 新增专用右面板类型、typed `CoatingAnalysisChanged` 事件以及通用 viewport surface-scalar 服务。
- `RobotQtViewer` 只负责创建模块、切换面板、转发 viewport 服务并显示 tooltip/status；未在 `MainWindow` 内加入厚度计算或着色逻辑。

## 行为与生命周期

- `Open Model` 继续使用 `SceneEntityWorkflowController` 修改项目文档，并使用 `ViewportReloadWorkflowController` 重建主视图；reload 失败会恢复导入前快照。
- 预测结果仅属于当前分析会话，不修改 project schema，也不设置项目 dirty 状态。
- `Show Thickness` 只控制约 30 Hz 的悬停 probe；预测后的热力图保持显示。
- 离开 Coating Analysis 时关闭 probe 并恢复原模型；重新进入时恢复当前会话 overlay。项目替换、目标删除或打开新分析模型时清理旧结果。

## 验证证据

- CMake configure/generate：`build/codex_coating_analysis`，Visual Studio 2019 x64，`BuildTests=ON`，通过。
- Release 构建：`RobotQtViewer`、`DemoThicknessPredictionTest`、`ScalarColorMapTest`、`PaintingAnalysisMeshAdapterTest`，通过。
- CTest：上述 3 项新增测试 3/3 通过。
- GUI 启动冒烟：`RobotQtViewerrx64.exe --profile-project config/projects/420-red4600-tool.sys.json --profile-exit-ms 1200`，退出码 0，OpenGL/项目场景初始化通过。

## 已知限制

- 第一阶段预测是确定性方向渐变与小幅横向波动，不包含喷枪、轨迹、遮挡或沉积物理模型。
- 精确 probe 当前只遍历 active analysis object 的三角形并做 30 Hz 节流；超大模型后续可在不改变公共契约的前提下增加内部 BVH。
- GUI 冒烟覆盖启动和项目加载；文件对话框与人工鼠标悬停仍属于交互验收项。

# 2026-07-21 OMPL 运动规划阶段 A-E 实施审计

## 变更目的

- 把 OMPL 接入现有 project/collision/runtime，而不是引入第二套 project。
- 在无 Qt 条件下先形成可测试的 headless 闭环和 GLFW 可视化中期里程碑。

## 主要变更

- `RobotCore`/`RobotIO`：补充关节 position/velocity/acceleration limit 数据契约和 URDF limit 解析。
- `SimulationRuntime`：增加批量关节更新，一次完成 FK 与 attachment propagation。
- `MotionPlanningCore`：增加与 backend/project 无关的 joint planning problem、scene validity、取消、统计和状态类型；保留旧线性规划接口兼容现有调用者。
- `MotionPlanningOmpl`：私有封装 OMPL 2.0.1 `RRTConnect`，实现固定 seed、终止条件、确定性简化、插值、时间化和最终逐段复验。
- `ProjectMotionPlanning`：从现有 `ProjectDocument` 构建任务私有 simulation/collision snapshot，严格验证 robot、joint limits 和所选 detector，并提供正式规划服务。
- regression/feature probe：新增真实项目 headless 回归与 GLFW viewer；没有把 project loader、FK、collision 或 attachment 语义复制到 viewer。
- 外部发现：OMPL 路径统一为 `D:/PreBuild/vs2019/ompl-2.0.1`，新增 `D:/PreBuild/vs2019/cmake/CMake_FindOMPL.cmake` 并由当前 Windows 用户配置引入。

## 行为与边界

- 规划期间使用任务私有 runtime，GUI/主 runtime 修改不会与 OMPL state checker 共享可变对象。
- 公共接口不包含 Qt、GLFW、OpenGL、FCL 或 `ompl::*` 类型；GLFW probe 和未来 Qt controller 消费同一服务。
- 默认运行不修改源 project；轨迹只有显式保存时导出，project 内存 round-trip 使用现有 `motion_planning.trajectories` extension。
- 第一版只接受 bounded revolute/prismatic joint state；TCP pose、IK、continuous joint 和动态障碍不在阶段 A-E 范围内。

## 验证证据

- Release 构建：`ProjectMotionPlanning-Headless`、`ProjectOmplPlanningViewer`、`RobotQtViewer` 通过。
- CTest：`ProjectMotionPlanningHeadless` 1/1 通过，覆盖缺失/禁用 detector、碰撞起点、预取消、固定 seed 可复现、直线碰撞、绕行、逐段复验、保存/重载和执行。
- GLFW：`--no-window` 规划/CSV 导出通过；`--hidden --max-frames 5` 完成 OpenGL 3.3/NVIDIA RTX 4090 初始化、渲染和退出。
- PE 依赖：`dumpbin /dependents` 未发现 Qt DLL。

## 已知限制

- 离散 collision motion validation 的安全性受 `maxJointStep` 影响，不等同于连续碰撞检测。
- 采样规划器对无法证明的无解空间通常以 `Timeout` 结束；实现不会返回空的 Success。
- Qt Workbench/document event/Robot Run 集成属于阶段 F，本轮未修改其业务接线。
