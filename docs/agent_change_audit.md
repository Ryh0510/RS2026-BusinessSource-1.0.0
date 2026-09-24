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

## 2026-09-23 导入关节路径的 APF 预修复

### 实现与所有权

- `ProjectMotionPlanning/src/ApfLocalPlanner.*`：私有人工势场积分器，目标吸引、原路径弱吸引、最近距离斥力、确定性切向脱困、正反向尝试、步长回退与有限迭代。没有采样树或 OMPL 兜底。
- `CdfQpTrajectoryRepair.cpp`：检测连续碰撞段，选取两侧安全锚点并有限扩展；验证重采样后的每条边；固定锚点及时间戳；完整预修复路径通过碰撞检查后才进入原有 QP/CDF。运动检查步长上限为 0.001 rad。首尾本身不安全且无法找到两侧锚点时拒绝，不再静默移动原始端点。
- 修复距离接口使用：`checkDetector` 的无碰撞结果可能不包含距离，改为复用该 detector 的完整过滤设置显式调用 `CollisionScene::distance`。无修改碰撞几何、障碍位置或碰撞对。
- `CdfDistanceField.h` 为私有距离场契约。最近特征的相对运动投影计算原 CDF 距离梯度，缺少可靠最近点时退回原中央差分。QP 目标、约束、参数和后处理保持。
- 连续关节统一解缠输出，使实际线性回放与检测使用的最短圆弧一致；锁定端点按关节的 2π 周期保持等价配置。新增真实几何的跨圈回归。
- 最终碰撞复验失败时不发布轨迹；无碰撞但安全间距不达标仍保持原来的 partial 状态，界面显示最终诊断。保存的 plan ID 规则不变。
- `ProjectCdfQpRepairOptions` 新增可选同步 `progress` 观察回调；没有新增依赖、项目 schema 或持久字段。通用起终点 OMPL 规划入口仍独立保留。
- Workbench 只更新 APF + CDF/QP 文案和失败提示；关节符号转换、导入导出、轨迹显示接线保持。

### 验证记录

- 当前测试输入：`C:/Users/14390/Desktop/ik_joint_angles.txt`，749 点，SHA256 `733BC8E1A230ECE4D6E3E135E8143A4E5DF5631D32459815D0E96AD4B97829AD`。场景 `config/projects/ABB4600-burnner.sys.json`。
- 初始端点均安全；0.005 rad 步长检查原始 748 条边，其中 277 条存在碰撞。加密到 8641 点后检测到 106 处连续碰撞区间，APF 预修复后全路径通过检查。
- 在输入中按固定步距抽取 38 个安全姿态，对最近特征梯度与独立中央距离差分比较，最大误差 `6.04401e-07`；全部样本启用最近特征加速。
- Release/Debug 均已编译规划 headless、APF 回归和 RobotQtViewer。Debug 仍有已有 OMPL 预编译库缺失 PDB 的 LNK4099 提示，不影响链接或运行。
- Release/Debug CTest：`ProjectMotionPlanningHeadless`、`ProjectMotionPlanningImportHeadless`、`ProjectMotionPlanningApf`、`ProjectMotionPlanningApfPeriodic` 均通过（4/4）。APF 回归覆盖无障碍、对称障碍脱困、端点锁定、确定性、完全阻断走廊、无效距离查询。
- 收紧步长的依据：0.005 rad 检查曾通过的结果，在 0.001 rad 独立复验中发现 2 段碰撞，因此这次不交付粗检查结果，生产管线的全部运动检查统一收紧到 0.001 rad。
- 最终真实输入回归使用 1 轮 QP，其余服务默认参数：8641 点，内部与独立实际线性插值（0.001 rad）均为 0 碰撞段；TXT 再次导入检查也为 0，返回码 0。全部原始时间点及 740.3255 秒时长保留；首尾配置周期误差 0；最大相邻角差 1.745515395 度。
- 轨迹点最小间距约 0.00675 mm，低于默认 10 mm；服务如实保留 success=false / partial，修复 CLI 返回 1 的原因是间距未收敛，碰撞与导出复验通过。计算耗时约 1192 秒，未更改默认 QP 迭代次数。
- 结果和日志：`build/apf-current-verified-joints.txt`、`build/apf-current-real-input.log`、`build/apf-current-export-reimport-check.log`、`build/APF-validation-report.md`（build 位于源码根目录的同级）。
- 原 Release EXE 被用户运行中的进程占用（LNK1104）；未关闭该进程。最新 Release 使用同项目、同 DLL 目录链接为 `build/Release/bin/RobotQtViewer_APFrx64.exe`；Debug 原目标正常构建。

### 复现

- 从 `build/Release/bin` 运行 `ProjectMotionPlanning-Headlessrx64.exe --project <source>/config/projects/ABB4600-burnner.sys.json --cdf-file <input.txt> --cdf-output <verified-output.txt>`。
- 添加 `--cdf-gradient-check` 进行独立距离梯度检查；添加 `--cdf-inspect --linear-input --verification-step 0.001` 按原始角度线性插值检查导出文件全部线段。可用 `--cdf-iterations 1` 跑一次 QP 回归；未更改生产服务和 GUI 的默认迭代次数。
- `ctest --test-dir <build> -C Release -R "ProjectMotionPlanning(Apf|ApfPeriodic|ImportHeadless|Headless)$" --output-on-failure`，Debug 同样执行。
- 检查为按关节步长采样的运动碰撞验证，不等同于连续碰撞检测；APF 不保证在任意复杂空间找到路径，失败时返回诊断而非碰撞轨迹。

## 2026-09-23 QP 默认单轮调整

- 将 CdfQpTrajectoryRepair.h、MotionPlanningEditorWidget.h/.cpp、headless main.cpp 中的 QP 外层迭代默认值统一为 1；界面原为 5，服务和 headless 原为 8。
- 用户仍可显式调整 Max iterations / --cdf-iterations。OSQP 内部迭代数及安全验证参数保持原有语义。
- 这是轮数设置修改；此前单轮真实输入完整流程约 20 分钟，不能据此承诺快速完成或安全间距收敛。
- 本轮验证：Release 领域服务、headless、MotionPlanningEditor 和另名 RobotQtViewer_APFrx64.exe 构建通过；未传 --cdf-iterations 的周期关节小样本仅输出 iteration 1，成功返回且无碰撞。此次未重跑完整 749 点样例，也未声称新的全量耗时。

## 2026-09-23 APF 轨迹减点与平滑改进

- 复核问题：原优化节点与 validationMaxJointStep 耦合；APF 积分路径未去绕行；旧平滑只微调单个点；QP 在目标、边界和初值中逐点折回连续关节角，导致跨正负 pi 的虚假大转角。
- 新增 optimizationMaxJointStep（默认 0.04 rad），最低插入点默认 1；独立保持最多 0.001 rad 运动检查，原输入节点时间戳和首尾配置不删除。实际 749 点样例加密为 4529 点，旧版为 8641 点。
- 私有 PathRefinement 在每个 APF 局部区间内使用有限前视捷径，每条捷径验证碰撞。随后在 QP 前后使用 32/16/8 点窗口、渐弱边界权重、时间加权弯曲代价和 0.15 rad 相对偏移限制进行平滑；代价包含两个接缝，仅接受长度不增加、弯曲代价降低且每条边无碰撞的候选。
- QP 后平滑若降低已有最小点间距（未达到目标时），整体退回平滑前轨迹；不会通过降低安全间距参数宣称成功。QP 仍默认一轮，线性回放与周期关节解缠规则保留。
- 原始文件前两行时间均为 0，但姿态不同。输出保留原时间；平滑内部对非正时间差采用最小正间隔，避免除零，同时输出诊断。这不是速度/加速度可执行性的保证。
- 小回归新增障碍圆弧去绕行/平滑、接缝代价、不可行窗口保持原值和无效时间拒绝；新增真实项目正负 179 度跨界并包含重复时间戳的回归，限制修正量低于 0.05 rad，防止 2pi 伪跳变复发。
- headless 增加 --cdf-gui-defaults 以匹配界面 trustRegion=0.02、seedCorridor=0.10、seedTrackingWeight=0.40、segmentIntermediateSamples=1；增加 --cdf-max-correction 供回归验证。

### 减点平滑验收结果

- Release/Debug 领域、headless、APF tests 和 GUI 构建通过，各 5/5 回归通过；git diff --check 通过。
- 真实输入使用 GUI 参数、一轮 QP：4529 点，规划 870.532 秒；内部、独立 0.001 rad 线性回放、导出再导入均 0 碰撞段。最小点间距约 0.02002 mm，低于 10 mm，仍如实返回 partial。
- 平滑前后关节弯曲代价 25914.1 → 5630.91 → 2907.07。历史输出与本轮输出同时间采样下的末端累计转弯角 1426.018 → 615.693，减少约 57%；两次优化的 GUI/服务参数有差异，不作单因素速度或质量提升结论。
- 保留所有原始时间戳和首尾周期等价配置；最大相邻角差 10.829 度，长边仍使用 0.001 rad 碰撞检查。没有执行速度/加速度/jerk 的时间参数化。
- 最新 Release 仍为 build/Release/bin/RobotQtViewer_APFrx64.exe；结果、对比图和方法说明见同级 build/APF-smoothing-report.md。完整回归之后只增加阶段进度日志及测试断言，数值算法未改。

## 2026-09-24 末端轨迹多逆解与 Basic Planning 调试

### 实现

- 保留旧 solveCartesianControlPoints，新增 solveAllCartesianControlPoints、CartesianMultiIkOptions/Result/Layer/Candidate，以及选解序列构造和实际模型限位读取接口。无新增依赖、持久 schema 或 OMPL/APF/CDF 流程变更。
- 直接读取导入的 cartesianControlPoints。实际 world FK 快照包含机器人基坐标和所选 TCP/法兰；继续沿用 Joint1/4/5/6 符号反转，未采用旧 DH 构造新候选。
- 每点沿用前一点所有几何根并加 64 个确定性 Halton 分散初值；每次最多 160 次阻尼迭代、步长限制与回退。找到一组后继续搜索。
- 周期几何根去重后在有限搜索窗口内枚举 2pi lifts，保留未折回角度与 turn；每组重新通过实际 FK 的位置 1e-6 m、姿态 1e-6 rad 阈值验证。旋转小数矩阵沿用 SVD 正交修正。
- GUI 输入每轴上下限（度）并与实际模型机械限位取交集，符号变换同时正确变换上下界。continuous 模型限位为无限，必须使用显式搜索窗口；默认各轴 [-180,180] 度。窗口不代表真实机械限位。
- 每点最多 512 个候选，最多 64 个几何根；达到限制显式显示截断。奇异位姿可能有无限解族，数值候选不声称数学完备或固定八组。
- Basic Planning 新增“全逆解”、搜索设置、取消和进度；新增“多解关节轨迹”主从视图，控制点下拉栏显示时间/候选数/播放解，表格显示六轴角度、turn、位置/姿态误差。编号仅在该点内有效，不伪装肩肘腕分支标签。
- 提供单点应用、设置当前点播放解、从当前解按未折回关节距离就近选取后续解、动态播放与停止。星号标记实际播放解；有无解点时禁止完整播放。
- 计算在 QThread 独占 FK 快照，主线程显示进度。项目、源轨迹、机器人、工具或相关预览改变时取消/清空；析构取消并等待；异常回到 UI。模块事件配置增加工具、附件、预览订阅。
- 多解候选是当前调试会话数据，不覆盖原始导入轨迹、不持久化所有候选。单点仍通过 document mutation，动态运行通过已有 runtime 接口并记录末端轨迹线。
- 多解动态播放属于逐点调试，不进行 APF/CDF、段间碰撞/速度/加速度验证，不强制同步构建网格碰撞场景。原普通关节轨迹播放的碰撞行为保留。

### 验证与限制

- Release/Debug 的 MotionPlanningEditor、ProjectMotionPlanning、RobotQtViewer、RobotQtViewer-SprayMeasurementSmoke 构建通过。
- Release 5/5：ProjectMotionPlanningHeadless、ProjectMotionPlanningImportHeadless、RobotQtViewerIkPlaybackSmoke、RobotQtViewerSprayMeasurementSmoke、RobotQtViewerMultiIkSmoke。Debug 后三项 3/3 通过。
- 回归覆盖实际模型多解、正负 turn、限位、FK 位置/姿态阈值、候选截断、无解层禁止播放、无实际 FK 拒绝、取消、源切换、应用符号、播放星号选择及逐点末端轨迹采样、旧单逆解与 TXT 导出。
- 初次 Debug GUI 测试复用了普通播放的同步网格碰撞场景构建，造成长时间无响应且一次被 Windows 关闭；另一次因固定 600ms 测试等待不足失败。已将多解播放定义为明确标注的纯构型调试，并把测试改为有上限地等待实际播放完成；最终 Debug 全部通过。
- 真实输入 C:/Users/14390/Desktop/11111.txt：749/749 点找到候选，5986 组，747 个点各 8 组，第 121 点 4 组、第 171 点 6 组，无截断。默认 64 分散初值全量搜索约 11.4507 秒（不含加载/UI播放）。
- 独立 FK 复验所有 5986 组：最大位置误差 0.000999176 mm，最大姿态误差 0.0000541424 度。对第 121/171 点使用 1024 初值重新搜索仍为 4/6 组；这不构成解完备性的数学证明。
- 日志与候选 CSV 在同级 build/multi-ik-11111.log、multi-ik-11111.csv；强化搜索记录为 multi-ik-probe.log、multi-ik-probe.csv。最终测试日志为 multi-ik-tests-release-final.log / multi-ik-tests-debug-final.log。
- 当前无原名 Release EXE 被占用，已正常更新 build/Release/bin/RobotQtViewerrx64.exe；历史 RobotQtViewer_APFrx64.exe 不是本轮新构建。
- git diff --check 通过；涉及 C++/CMake 文件全部保持 CRLF。

- 最终 RobotQtViewerrx64.exe 隐藏启动 smoke（--smoke-exit-ms 1800）返回 0。

## 2026-09-24 多解结果被显示刷新误清空

- 用户报告单点应用一次后多解栏消失，后续调试按钮不可用。旧回归只订阅 ProjectDocumentChanged，缺少实际应用中 ToolSetup 的二次通知，未覆盖此问题。
- 源码确认：单点应用发出 motionPlanningMultiIkApply 文档变化；ToolSetup 刷新后同步 pinned frames，使用独立 sourceId toolSetupPinnedMountFrames 发出 ViewportPreviewChanged。旧 controller 对所有 preview 事件调用 invalidateMultiIk，因此原 apply sourceId 例外不起作用。
- 修复仅在 preview 包含基座、mount、object frame 或 scene object 几何变换时使多解失效。坐标系显示、固定坐标系、焦点及纯显示预览清理保留候选和选解。真实项目/轨迹/机器人/工具更改仍按原规则失效。未修改求解算法、关节符号、限位、应用 mutation 或播放数据。
- 回归补充真实事件链同等的 ToolSetup 嵌套 pinned-frame 通知：修改前 RobotQtViewerMultiIkSmoke 在“Single candidate applies with original signs and retains multi results”处失败；修改后验证连续应用不同解、所有点/候选保留、显示/焦点刷新、切换控制点、选解与动态播放，以及基座变换仍清空旧解。
- Release 三项回归（MultiIk、IkPlayback、SprayMeasurement）通过；修复版隐藏启动 smoke 返回 0。日志为 build/multi-ik-retention-before.log、multi-ik-retention-release.log、multi-ik-retention-startup.log。
- 用户原 RobotQtViewerrx64.exe 正在运行，未终止或覆盖；当前交付 build/Release/bin/RobotQtViewer_MultiIKFixrx64.exe，与现有 DLL 同目录。

- Debug 主程序和 smoke 目标构建通过，RobotQtViewerMultiIkSmoke 通过（43.58 秒）；日志 build/multi-ik-retention-debug.log。源码/CMake 保持 CRLF，git diff --check 通过。

## 2026-09-24 分层图 Top-M 与 CDF 初始解传递

### 领域与算法

- 新增 ProjectMotionPlanning/LayeredIkGraph.h/.cpp，公开 LayeredIkGraphOptions、LayeredIkPath、LayeredIkGraphResult 和 ProjectLayeredIkGraph::filter。消费现有已通过实际 FK/限位校验的 CartesianMultiIkResult，不重新生成末端位姿或逆解。
- 层节点为原候选；相邻层隐式完全二部连接。所有首层前缀代价为零，只累计 sum(w[j] * (q_next[j]-q_prev[j])^2)。直接使用含 turn 的实际弧度值，无 wrapToPi、节点代价、碰撞调用、速度或加速度硬约束。
- 每节点保留前 M 条前缀，以每个前驱的有序前缀列表加边代价后做堆归并；最终再归并末层并回溯，得到全局代价最小且候选索引序列互异的 Top-M。多条序列可以共享同一个首点构型；不以首点分组截断。相同代价按节点和前缀序号稳定排序。
- API 默认 M=30，允许 1..1000；GUI 可配置 1..200、六轴非负权重。前缀存储默认预算 8,000,000 条，分配前检查；超限明确失败，不裁边、不减少输入候选、不返回冒充精确 Top-M 的近似结果。
- 支持取消和进度，任何无解层、未完成逆解、非法数据/权重或代价溢出均失败。若组合总数小于 M，则返回实际全部序列；一层轨迹得到每候选一条零代价序列。逆解曾截断时传递提示，排名范围仅是已找到的逆解集合。

### 界面与后续流程

- Basic Planning 新增“分层图筛选”、M 和 J1..J6 权重；结果栏显示排名/总代价/点数/首末解编号，选中后显示每一个原控制点的时间、解编号、六轴度数和 turn。
- QThread 使用独立输入副本，支持取消；修改参数或源多逆解失效时清空旧排名并取消任务，完成回调不发布已取消结果。窗口析构取消并等待任务结束。
- “使用该结果作为优化初始解”从选中完整序列构造轨迹，验证机器人和关节顺序；不通过 TXT 或表格显示值中转，以全精度原时间/原关节值转换到度数后填入现有 m_cdfJointPoints，并切换到 CDF 页。原始多逆解和分层图结果保留，未自动运行 APF。
- CDF 单点应用和后续 repairImportedCdfTrajectory 复用既有关节符号映射。修复流程排序改用 stable_sort，避免重复时间戳交换控制点；无 QP/CDF 参数或算法变更。
- CDF/普通关节单点应用均属已知仅关节状态变化，保留多逆解/排名；几何或源轨迹变化仍使结果失效。分层图传入的 CDF 初始解绑定所选机器人，切换机器人或打开项目会清空该临时输入，防止误用于另一机器人。
- 本轮不进行碰撞后的最终 Top-K 重排，也不自动优化全部 M 条。用户本轮所称前 K 组输出，对应界面设定 M 条运动学候选；用户选择一条交给现有 APF/CDF。
- 无新增依赖、项目 schema 或持久化字段。

### 验证

- 新建领域回归 IkGraphTests：小图穷举比对 Top-1/7/30、加权代价与排序、序列唯一、零权重同价稳定性、360 度位移不折回、选解回溯时间/角度保持、单层/空层、非法权重/数据、预算拒绝及运行中取消。新测试 target 名缩短以避开 Windows 260 字符中间路径限制。
- GUI smoke 验证 M=30/3、选中排名的完整明细、逐点/逐轴 CDF 输入对照、CDF 实际关节应用符号、结果保留、参数更改/源失效/取消后的禁用行为；保留旧 IK、末端轨迹、TXT 导出与多解通知链回归。
- Release：主程序、smoke、IkGraphTests 构建通过，相关四项 CTest 4/4 通过。Debug：同目标构建通过，领域与多逆解 GUI 两项 2/2 通过；既有 OMPL Debug PDB 缺失警告仍存在。
- 真实输入 C:/Users/14390/Desktop/11111.txt：749 层、5986 个已验证逆解，Top-30 用时 0.0094875 秒（仅图阶段，不含约 11.66 秒多逆解）。返回 30 条不同完整序列、每条 749 点；代价范围 148.219446481662..152.738179469245 rad^2，独立重算一致，所有原点和时间保持。
- 真实候选逐点报告：同级 build/topm-11111.topm.csv（22470 行数据）；全量日志 topm-11111.log。构建/测试日志 topm-release-build.log、topm-debug-build.log、topm-tests-release.log、topm-tests-debug.log。
- 未对 30 条执行完整 APF/CDF 耗时优化，不把运动学排名当作无碰撞或工艺位姿保持的保证；本轮验证到 CDF 原始输入和实际单点应用入口。
- 已正常更新同级 build/Release/bin/RobotQtViewerrx64.exe 和 Debug 程序。旧另名 MultiIKFix/APF EXE 不包含本次新增功能。

- 最终主程序隐藏启动 smoke（--smoke-exit-ms 1800）返回 0；git diff --check 和修改源码/CMake 的 CRLF 检查通过。

## 2026-09-24 查看构型选择窗口

- Basic Planning 分层图排名表下新增“查看构型选择”，有有效排名时启用。controller 从完整 `LayeredIkGraphResult::paths[].selections` 转为一基编号只读投影；不从抽样表格或当前播放序列拼接曲线。
- 新增 `ConfigurationSelectionDialog`（MotionPlanningEditor 内 Qt 视图）：任意候选勾选、全选/清空、稳定颜色/线型图例、同图叠加、滚动分行对比、控制点起止区间和全程恢复。QPainter 逐点画线，不降采样；单点范围仍画标记。
- 下方显示当前区间内所选序列存在选择差异的点数。坐标均一基，与现有逆解明细一致；明确说明逆解编号是本点内候选编号，不是跨点稳定的肩/肘/腕标签。
- Widget 以 QPointer 管理无模态窗口，重复点击复用。重新筛选、改变参数或源结果失效时关闭并清除旧快照，防止排名和图形不同步。
- 保留既有未提交 Top-M 实现；本轮未更改图筛选、IK、APF/CDF、播放或项目持久化语义，无新增第三方依赖。
- GUI 回归逐一核对所有排名、所有点的图形数据与 controller 明细；验证窗口复用、参数失效、重算后的新快照。合成 749 点回归覆盖单点差异、49 点区间差异、任意多选、全选/清空、单点/逆序区间、分行/叠加和窗口释放。
- CMake configure、Release/Debug 的 smoke 和主程序构建通过。Release 三项 GUI CTest 3/3；Debug MultiIkSmoke 1/1（48.88 秒）。Debug 仍有既有 OMPL/FCL PDB 缺失 LNK4099 警告，无构建错误。
- 独立绘图测试通过，已查看 `../build/config-selection-preview.png`，可见孤立单点差异。Release 主程序 `--smoke-exit-ms 1800` 启动退出码 0。
- 日志：同级 build/config-selection-{configure,release-build,debug-build,release-tests,debug-tests,plot-test}.log。7 个本轮源码/CMake 文件均 UTF-8/CRLF，根仓及 Workbench 子仓 `git diff --check` 通过。
- 使用当前更新的 `../build/Release/bin/RobotQtViewerrx64.exe`。说明见 `docs/multi_ik_debug_usage.md` 的“查看构型选择”。
