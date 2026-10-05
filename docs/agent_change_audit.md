# 当前变更审计

## 2026-10-04 当前11111可行路径与独立验收

- 在用户允许起终使用其他有效逆解后，直接复用原始Cartesian位姿、实际TCP FK及现有IK连续迭代。B1起终种子保留合法连续轴圈数，绕开原Top-1最终B2腕部回接限制。完整APF＋单轮QP输出4528点，原始符号与100mm约束不变。
- 最终导出TXT独立实际模型/新碰撞场景复验60941个原始关节线性采样，APF另复验130568采样：0碰撞/0限位违规，有序TCP最大偏差99.99986064/98.10077549mm，原始起终位姿匹配。最终离散动态限制通过。
- 默认10mm余量未达到，success仍为0/_partial；这是满足当前碰撞采样和100mm位置走廊的几何路径，不是达10mm安全余量、完整中间姿态、完全光滑、全局最优或实机认证。没有修改参数以改变成功标志。
- 结果/复算初始解/图表/验收/校验和存于外层results/11111_APF_100mm。用户通过现有CDF导入/播放使用；未自动替换原Top-K当前选中结果。
- 新增两个实际回归CLI入口用于候选生成与独立导出复验，UTF-8/CRLF；诊断新增Eigen模板触发Debug对象节限制，仅该回归目标增加MSVC /bigobj。其他前轮未提交改动均保留。


## 2026-10-04 APF 有序参考与可配置 TCP 硬门限

- 用户先授权 50 mm，随后改为 100 mm；最新默认 100 mm。保留上一轮所有阶段诊断、二阶 QP、定时及播放修改。
- 根因：仅终点吸引/全局捷径/弧长重分配可跳过扫描线；真实 Top-1 初始关节插值本身对 TCP 控制点折线偏移约 942.902 mm。
- 领域修改：按参考站点推进、局部 TCP 前瞻/DLS/切向场、参考平面截断与偏移衰减；移除捷径/重采样；原始 Cartesian 折线/加密对应、实际 FK 高密度走廊校验覆盖 APF、QP 回溯、局部修复、平滑及最终输出。相邻窗口包含绝对起始下标，避免错配扫描线。无新增依赖。
- Workbench 增加默认 100 mm 参数及绿色原始控制点折线；明确位置/姿态指标的不同参考。未通过的 APF 阶段显示为诊断，失败路径不作为优化结果存储。公共选项需真实 TCP FK；headless 旧工具仅显式关闭位置限制，不能伪称已验收 50 mm。
- 验证：Release/Debug 构建、各 10/10 CTest、主程序启动通过；有序之字形/turn/固定锚点/正反向/走廊可行与失败/非线性 FK 中点/缺失 FK/平滑约束回归通过。实际 Top-1 在 1360→1403 加密节点未找到 50 mm 内连接，正确不启动 QP、不发布结果；不宣称已解决其完整可行性。
- 最新100mm复验：通过原50mm失败区间，但在1660→1813加密节点仍未找到连接，322.4秒后明确拒绝，QP=0/优化输出=0；实际GUI回归退出0。100mm的Release/Debug各10/10与主程序启动通过。
- 本轮 14 个源文件，不改 SDK 或项目格式；默认仍一轮 QP。详细行为、限制、证据与新 EXE 哈希见 docs/cdf_apf_shape_corridor.md。

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

## 2026-09-28 按起点构型独立 Top-K 与双页对比

### 算法与结果归属

- `ProjectLayeredIkGraph::filter` 接口及全局 Top-M 语义保留；新增 `filterByStart`，以首层每个候选编号固定起点，复用相同精确动态规划计算每组 Top-K。后续层仍完整连接，仍只有实际含 turn 的加权关节位移边代价，不添加碰撞/动力学裁边。
- 共用内部实现仅改变首层可达前缀，跳过没有前缀的起点；原始候选下标不重排/重编号。按起点编号、组内代价顺序返回，组合不足 K 时保留全部实际组合。起点数量取自输入，不能假定总是八种。
- 前缀预算按实际可达前缀检查，另外限制累计输出索引数量；预算失败/取消均不发布不完整分组。小图穷举覆盖 K=1/7/30，验证排序、去重、原索引和代价；验证零权重同价顺序与全局一致、多圈不折回、单层、空层、跨组取消与输出预算。
- Workbench controller 后台任务先计算原全局结果，再计算固定起点结果，分别持有领域结果。原全局计算成功而新增阶段失败时保留全局结果并报告新增阶段失败，不伪造分组结果；用户取消仍不发布两页部分结果。

### 界面

- 新增独立参数“每起点保留 K 条”，默认 1、范围 1..200；原全局 M 保持默认 30。结果栏双页：“全局 Top-M”与“按起点 Top-K”，有明确的排名依据说明；共用完整逐点明细和应用按钮按当前页/当前行工作。
- 第二页显示起点逆解、组内排名、全局排名、总代价、点数、末点逆解。通过完整索引序列查找原全局列表，匹配到显示精确排名；列表外只显示 `>M`，明确未计算更远的具体排名，不把它误写为 M+1 或组间排名。
- 两页结果都能通过原符号/度数/时间映射传入 CDF 初始栏，来源包含全局排名或起点/组内排名信息，后续 APF/CDF 流程保持原样。
- 对比对话框重用绘图页组件，增加同名两个分页，接收两套完整只读投影。第二页默认勾选每组最佳，可选任意组/组内排名；区间、多选、分行状态各页独立。第二页图例分行显示全局排名，避免横向截断。
- 参数/源变化同时清空两页并关闭旧绘图；打开绘图时切到结果栏当前类别。保留原单数据集绘图构造接口供兼容调用。
- 无新增依赖或持久化 schema；领域 filter 兼容，Qt 内部信号新增结果类别和每起点 K 参数，以避免跨页引用错误。

### 验证与交付

- CMake configure、Release/Debug 领域/GUI/main application 构建通过。Release 领域回归及三项 GUI 回归通过；图例显示调整后再通过三项 GUI 回归。
- GUI 测试逐点核对两页完整图形数据；默认 K=1 覆盖每个实际起点、全局名次/范围正确，每个起点的最优序列都按时间及六轴数值核对 CDF；另测独立的 M=3/K=2、分组编号、双页失效与重算。旧绘图回归改为按页面定位同名控件，避免跨页误取。
- 真实 `C:/Users/14390/Desktop/11111.txt`：749 层、5986 候选；多逆解约 16.685 秒，全局 Top-30 约 0.0117 秒，固定 8 个起点各 Top-3 得到 24 条完整序列，新增计算约 0.0215 秒。所有时间/候选索引保持，实际未折回关节差代价独立重算通过。
- 各起点组内第 1 名代价（rad^2）：#1 175.058841、#2 168.188251、#3 170.125532、#4 172.735699、#5 168.794541、#6 172.429832、#7 161.607382、#8 148.219446。#8 匹配全局第 1，其余均在全局前 30 之外。
- 数据和日志位于源码同级 build/start-topk-11111.start-topk.csv（24×749 行）、start-topk-11111.topm.csv、start-topk-11111.log；已检查真实绘图及图例调整后的截图。其余日志前缀 start-topk-。
- 原 Release EXE 正在运行，未关闭用户进程；用既有 MSBuild TargetName 覆盖生成 `../build/Release/bin/RobotQtViewer_StartTopKrx64.exe`，使用说明已更新。原 EXE 暂未覆盖。新版启动 smoke 退出 0。
- 本轮 10 个 C++ 源码/头文件保持 UTF-8/CRLF，三仓相关 `git diff --check` 通过；未提交更改。

- 最终 Debug 领域与多逆解 GUI 两项 CTest 2/2 通过（41.44 秒）；Debug 仍有既有第三方 PDB 缺失 LNK4099 提示，无新增构建错误。

## 2026-09-28 CDF/QP 等价提速

- 瓶颈通过原版完整进度日志确认：大部分耗时在重复距离和高密度运动验证，候选/回溯每次约 45–48 秒；“开始 QP”到“候选验证”的时间还包括当前轨迹验证，不能全部当作 OSQP 求解器时间。
- 新增私有 CdfQueryBatch：最多四个默认独立场景并行查询，单次 repair 精确 double 缓存（各 32768 项），不共享可变碰撞运行时或跨场景物体 ID；参数、状态、检测步长均纳入缓存键。有限资源降为较少场景，批量任务返回前完成。
- 距离、梯度、全路径验证与平滑窗口边验证接入；平滑窗口顺序和数值算法保持不变。失败的距离接受条件提前短路掉无用运动查询；最终全路径强制绕过缓存，保持 0.001 rad 新鲜检查。
- 新增 queryWorkers 执行选项（0 自动、1 串行），headless 提供 --cdf-workers 和 --cdf-query-check；GUI 默认自动使用，无 UI/项目 schema 变化，无第三方依赖变化。其余 APF/QP 接口和约束保留。
- Release/Debug 领域、headless、APF tests、主程序构建通过，各 6/6 CTest 通过。新增回归核对串行/并行/缓存/新鲜结果、梯度、参数键、微小角差、非法值；最大梯度差为 0。
- 保留旧版 CdfSpeedBaseline.exe，顺序运行同一 749 点文件和 GUI 默认参数。一轮 QP 全程 861.754 → 316.912 秒（减少 63.2%，约 2.72 倍；计时包含输入预检查，不包含独立末尾复验）。
- 两版输出均 4529 点，导出 TXT 逐字节相同，SHA256 均 a13900ef8438c18879f1d0269573c2ce3d84faeae8b865daa49f7f86c3c8f833；独立 0.001 rad 回放均 0 个碰撞段。原有安全间距未达标提示/返回 1 保留，未把提速混同为优化已收敛。
- 本次精确距离缓存命中 12072，运动缓存命中 11521；保留单线程 APF、全部原点和时间戳、平滑规则、QP 矩阵/容差/迭代设置。
- 交付更新的同级 build/Release/bin/RobotQtViewerrx64.exe；主程序启动 smoke 退出 0。此前另名 StartTopK EXE 未覆盖。详细基准、限制和复现命令见 docs/cdf_qp_performance.md。
- 本轮 8 个源码/配置文件保持 UTF-8/CRLF；相关 git diff --check 通过。工作树未提交。

## 2026-09-28 Top-K 输入耗时复核

- 用户确认慢输入来自 Top-K，并非上一轮 ik_joint_angles.txt。当前保留此前提速的所有未提交修改。
- GUI 旧入口同步占用 UI 线程且未接 progress；改为 controller 私有快照后台执行和模态阶段窗口，记录 EXE、配置、输入来源、阶段时间。期间文档变化则拒绝提交过期结果。
- APF 梯度探针、碰撞区间扫描和局部 QP 查询复用领域层独立场景批量查询/精确缓存；保持 APF 搜索、有限差分、QP 参数及最终 0.001 rad 新鲜复验。
- 用现有全局 Top-M 第 1 条 749 点候选进行顺序对照；实际用户所选编号尚未明确，不能把该复测当成其具体运行。验证包括 APF 批量等价回归、查询回归、Release/Debug 主程序构建。

- 本轮完成：全局 Top-M 第 1 条实测 1012.320 → 439.964 秒；难段 1660→1813 为 726.794 → 277.199 秒。两版 4592 点导出 TXT 逐字节一致，SHA-256 为 c73a1ea0998ec31a7b4fd5ae2e5bfb579fe0bc5d1cbdc3e8cdfc811b2a4381ce；独立 0.001 rad 复验均 0 碰撞段，安全间距未达标的返回 1 状态保持。
- CMake configure、Release/Debug 主程序与相关目标构建通过；两配置各 6 项领域回归及新 GUI 阶段/线程/过期结果回归通过。阶段窗口已视觉核对，主程序 smoke 退出 0，源码 UTF-8/CRLF 与 diff 检查通过。
- 当前 GUI：同级 build/Release/bin/RobotQtViewerrx64.exe；每次运行日志位于 EXE 同目录 log/cdf，结束摘要给出路径。完整证据/口径见 docs/cdf_qp_performance.md 第二轮；未把第一轮原始文件基准推广为任意 Top-K 的耗时保证。未新增公共 API、第三方依赖或项目持久化字段。

## 2026-09-30 ABB4600 固定肩/肘/腕构型显示

- 根因：多逆解按实际关节向量排序，图中使用候选行号；行号在不同目标点不能代表同一运动学分支。
- ProjectMotionPlanning 新增 CartesianIkConfiguration、可选分类回调及实际模型分类快照工厂。S/E/W 来自实际关节轴和腕中心几何，沿用原符号映射；规则及 B1～B8 对照见 ik_configuration_branches.md。三分支边界显示 0，未知模型不猜测；不使用旧理想 DH，也不是厂家 confdata。
- 每个候选在原 FK/限位验证后附带分类；按固定分支、turn、关节值排序。未分类时保留原数值排序；不会合并多圈解、改变关节角、删图边或加入节点代价。追加领域接口/结果元数据，既有单逆解接口保留，无新增依赖或项目存储格式。
- Workbench controller 统一投影分类；全逆解表显示 B 标签与调试候选序号。两页图结果、逐点明细、对比窗口同步显示，单点应用/播放/CDF 仍以原候选身份取值。对比图默认按分支绘制阶梯线，可恢复候选视图；悬停显示 turn，摘要区分候选差异与分支差异，轴宽随字体测量。
- 真实 11111.txt：749 点、5986 候选，8 种分支；普通八根层均一一对应八个标签。第 121 点为 B1～B4；第 171 点为 B1、B2、B3、B4、B5、B7，不把 B7 重编号成 B6。候选独立实际 FK 最大位置误差 0.000999176 mm、姿态误差 0.0000541424 度。全局 Top-30、每起点 Top-3 共24条均保留原时间、关节值和独立计算的代价。
- 回归覆盖整圈不变性、世界基座刚体变换不变性、腕部边界显式处理、原符号、连续多次应用、选解播放、双页图表/CDF 传递、源失效清理和导出。全量报告在同级 build/branch-final-real.csv、branch-final-real.log。
- GUI 在 QT_QPA_PLATFORM=offscreen 下退出 0xc0000409；该 OpenGL viewer 的验收改用原生 Windows Qt 平台，相关测试正常通过。未把 offscreen 失败当成功，也未修改渲染后端。

- 完成：CMake configure、Release/Debug 主程序和相关测试目标构建通过，两配置各四项回归通过；真实 749 点完整 FK/构型/Top-M/按起点序列回归通过。新增分支图和候选表已截图检查，修复了长标签裁切；最终 Release 启动 smoke 退出 0。
- Release 原 EXE 被运行中进程占用，保持该进程，按同项目 TargetName 覆盖另存 `build/Release/bin/RobotQtViewer_Branchrx64.exe`。SHA256：`2D2D055FD190E67680C42122538E75803FC37DF57931908ECFAC92BF80EFC679`。Debug 为正常 `RobotQtViewerdx64.exe`；其既有 OMPL 缺失 PDB 警告不影响构建/运行。
- 界面预览：同级 build/branch-ui-final.png；测试日志：branch-final-test-release.log、branch-final-test-debug.log、branch-delivery-test-release.log、branch-delivery-test-debug.log。源码 UTF-8/CRLF 和根仓/两子仓 diff 检查通过。

## 2026-09-30 按起点 Top-K 结果表切换卡顿修复

- 用户最终截图确认故障位于 Basic Planning 主面板的分层图结果分页，K=1。卡住进程 UI 线程栈为 QTableWidget 模型 dataChanged → QAbstractItemView::update/visualRect → QHeaderView::resizeSections → QTableView::sizeHintForColumn → QStyledItemDelegate::sizeHint → 字体布局。
- 根因：明细表开启 ResizeToContents，逐格填充时仍通过模型通知反复扫描列内容；setUpdatesEnabled(false) 只能禁用绘制，无法阻止尺寸测量。已有明细再次替换时出现近似平方级工作量。
- 修复归属 MotionPlanningEditorWidget 的临时显示层：三个分层图表共用 replaceLayeredGraphRows，批量更新期间固定列几何并阻止表级选择通知，填充完成后一次性恢复自适应列宽。不屏蔽底层模型通知，不更改候选、排名、构型分类、关节值或 CDF 初始解映射。
- 新增 RobotQtViewerConfigurationTabsSmoke，使用主程序真实主题与中文标签，30 条全局结果、8 个起点 K=1、每条 749 点明细，覆盖连续选行、窄面板缩放、双页切换、末点数据和使用结果信号；60 秒超时防回归。此前只有短轨迹 GUI 测试，没有覆盖长明细替换。
- 同一界面回归的旧版在第二个起点明细替换处 30 秒超时；修复版 14 次完整明细替换各约 27–31 ms（本机 Release、合成完整长度 UI 数据，不代表 IK/APF/QP 计算耗时）。日志同级 build/topk-table-before.log、topk-table-after.log，截图 topk-table-after.png。

- 验收完成：Release/Debug 主程序与 smoke 目标构建通过，两配置各 5/5 回归通过；新版 Release 启动检查退出 0，截图视觉核对及 UTF-8/CRLF、git diff --check 通过。运行中的原 EXE 未覆盖、未终止；交付同级 build/Release/bin/RobotQtViewer_TopKFixrx64.exe，SHA256 4B84F3C0013B718EA8C4B0902AC093E00D253C086A44ECCF429A310B728EFAB0。测试日志 topk-fix-tests-release.log / topk-fix-tests-debug.log。无新增公共 API、第三方依赖或持久化字段。


## 2026-10-02 合并 Business Source 1.0.5

- 用户明确授权比较指定两个版本目录并把新版内容更新到现有目标。原根 HEAD d7c1594、规划算法子仓 eb15cde、规划工作台子仓 47ac0b1，开始时工作树干净。目标虽命名 1.0.0，原 SDK 实际 1.0.3；以发布包 1.0.3/1.0.4 辅助三方比较，保留真实定制。
- 来源 2432 文件中相同1968、同路径不同363、新增101；另有3493个目标独有文件。完整比较和处理清单在外层 build/upgrade-105，最终人类说明见 upgrade_1_0_5_report.md。
- 业务部分74个新增、147个更新（不含本轮说明文档）；PrebuiltPackages 成套升级到1.0.5，1875文件逐个SHA256一致，原SDK的506个额外文件保存在备份而非混入新SDK。原740个业务文件及整个SDK已备份；data、thirdparty、用户工程、现有build输出保留。
- 合入 Camera-on-Hand、CustomMesh/高度场、Collision新接口、Project事务/runtime session、场景系统拆分、工作台贡献/生命周期、界面/渲染/本地化、新示例与测试。
- 本地运动规划接入新增窄 MotionPlanning viewport port；喷嘴标定迁入新 runtime 引用结构并补全复制/移动；48个算法与关键UI文件对比备份无变化，保留全逆解、肩肘腕/turn、双页Top-K、APF/单轮CDF-QP、后台进度、轨迹线和分页卡顿修复。
- 修复消费工程缺失ShaderResources DLL部署、外部SDK数据根指向原开发机、旧工作台名称重复；旧碰撞测试迁移typed port/project asset store并使用自包含STL。Business Source架构门禁只检查可见业务源码，明确跳过未分发的私有源码；不伪造SDK内部验证。
- C:/b/rs105-merge 全量启用目标 Release/Debug 构建成功，两配置主程序默认ABB4600启动退出0。Release31项回归最终均通过；CDF GUI专项通过。Debug31项最终均通过：30项全套通过，查询测试首轮180秒预算不足，使用900秒预算专项复验，195.78秒退出0、最大梯度差0。
- 真实11111.txt 749/749点、5986候选、0层截断，最大FK位置误差0.000999176mm、姿态0.0000541424度；全局Top30、8起点各Top3通过独立路径代价/时间/关节值验证，单点应用/播放/CDF传递/导出保持正确。数值多种子不宣称全逆解完备。
- 未新增第三方依赖；新增应用窄接口，公开SDK接口由1.0.5包提供；项目version3保留。208个变更源码/构建文件UTF-8/CRLF及冲突标记检查通过。
- 新Release EXE：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 34C353815433C6E894654E04536D5E6E4818FEDD31526940EF136BCC71294B1A。需连同当前目录DLL/config使用，旧EXE仍在原build，不直接覆盖。


## 2026-10-02 修复 CDF/QP 结果播放卡顿及切换工作台重载

- 用户报告升级后播放优化结果导致主视口卡顿、动态播放切换模式后中断。开始时根与规划工作台子仓干净；仅修改相关适配器、ViewerCore、MotionPlanning 控制器及回归，不修改规划算法/数据格式。
- 重载根因：MainWindow::enterWorkbench 每次调用 clearTaskPreview，其中 clearAttachmentBindingPreview 原本无条件重载整个 ProjectDocument。装配适配器现在记录是否真的创建过绑定预览；没有预览时清理不触碰运行态。创建/取消真实预览仍走既有加载路径，正式文档加载清除标记。相机、运行时关节、轨迹线不会因单纯工作台切换而重置。
- 播放开销：原 advanceJointPlayback 每点反序列化全部 StoredMotionPlan，六关节逐个应用重复更新所有机器人及工具；startJointPlayback 还会刷新并强制开启实时碰撞查询，而每点已有独立碰撞快照校验。
- 修复：每次播放建立不可变共享轨迹快照，停止时释放；防止同步事件中途停播导致引用失效。整组关节经 typed port → RobotViewport → ProjectScene → SimulationRuntime 批量应用，普通机器人同步一次；并联机构保留已有耦合更新路径。预检查维度、有限值及关节存在性，失败不静默继续。单关节接口保留。
- 播放不再强制改变视口碰撞开关或激活检测器；每个原始点仍由独立 ProjectPlanningSceneSnapshot 校验，显式开启的视口碰撞设置保留。没有抽点、关闭播放碰撞校验或改动 ABB 符号。真实文档/几何变化终止旧快照，纯工作台预览清理继续播放。
- 新增回归：2001点完整播放，20次真实 viewport preview 事件链清理，检查无重载/姿态重置、最终关节、整组 TCP 采样、非法关节组拒绝、文档变更停播；IK测试接入真实 collision port 检查播放不强制开启重复查询。--optimized-playback <project> <txt> 可复用实际优化结果验证，不依赖重新跑 QP。
- C:/b/rs105-merge 的 Release/Debug 主程序、喷涂回归及碰撞工作流目标构建通过；两配置各9/9相关回归通过（架构、碰撞工作流、工作台生命周期、文档事务、视口呈现、Top-K分页、IK、多IK、喷涂播放），主程序 --smoke-exit-ms 2500 两配置退出码0。UTF-8/CRLF 和 git diff --check 通过。
- Release 读取外层 build/cdf-top1-after-joints.txt 的4592点实际 ABB4600 轨迹：完整播放46.451秒（测试以1ms定时请求逐点播放，非实时节拍保证），20次预览清理、0场景重载，4592/4592碰撞校验、0碰撞/0无效，最终符号映射关节一致；20ms界面心跳最大间隔67ms。此数据包含喷涂测量、末端轨迹绘制和碰撞采样，未测原版同场景总时长，因此不宣称具体加速倍数。回归触发的是工作台切换使用的同一预览清理事件链，并非人工逐一点击所有工作台。
- 日志位于外层 build/playback-fix-{configure,build-release,build-debug,build-debug-tests,tests-release,tests-debug,real-cdf-release,app-release,app-debug}.log。Release 可执行文件 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 54F3734F93A54586E727E47742BCCF4C4B932EF3E6B9F65552AC90633E55BF62。沿用此目录配套DLL，不覆盖旧SDK目录下的EXE。


## 2026-10-02 再次修复 CDF 回放动作不连贯

- 用户确认运行新版本，主要症状是机器人动作不流畅。保留前次未提交修改，重新定位后确认前次只检查界面心跳和样本完整性，不能据此确认运动节奏正确。
- 原播放每次定时回调只推进一行，4592点在1ms请求间隔下实际需要46.451秒；每点耗时直接改变播放速度。该实际文件相邻关节段长约0.024至14.17度，等间隔逐点应用会忽快忽慢；两处重复时间戳也不能直接当作零时长跳变。
- MotionPlanningCore 新增 JointPlaybackTimeline：普通有效轨迹按原时间比例缩放，CDF预览按关节空间折线弧长分配预览时间；非递增时间回退到弧长。点间线性插值，不wrap角度，不改原q/time/导出。验证有限数、维数、重复点、单点、turn。此为预览节奏，并非重新生成物理执行速度/加速度受约束的轨迹。
- MotionPlanning 控制器每16ms推进显示，按单调时钟插值；每帧处理全部到期源点并保留逐点碰撞/喷涂/轨迹采样，合并界面通知到20Hz；8ms批次预算避免追赶过期点长期占用GUI，过载放慢显示而不丢采样，长时间事件阻塞恢复时最多推进50ms预览时间。喷涂数据记录原始点，插值帧不混入原始行数。
- ProjectScene 原逐段 DebugDraw::drawLine，每段内部有1000点Trajectory缓冲，因此数千段每帧反复创建和销毁大量对象。新增私有 EndEffectorTracePass，增量维护完整折线，通过已有 SDK TrajectoryPass 一次绘制；保留颜色、Gizmo过滤、显示/清除语义及全部轨迹点，没有简化或抽点。初次图像回归发现SDK默认Trajectory过滤不包含Gizmo，已显式设置正确过滤后复验图像通过。
- 新回归覆盖稀疏点间机器人实际运动、弧长速度与采样密度独立、源时间比例、turn、重复时间/点和非法输入；实际CDF回归增加可见规划面板、SceneExplorer运行事件订阅，以及实际绘制帧间隔统计。2001点按1秒预览约1.024秒完成，全部原始样本和20次模式清理事件通过。
- 4592点实际ABB4600压力回放（请求0.1秒，为验证过载时不丢点）从上一轮46.451秒降到中间版本7.956秒，折线批量绘制后5.245秒；4592/4592校验、0碰撞/0无效、0场景重载，原最终角度一致。此为指定测试场景的回放耗时，不代表QP优化耗时或所有工程固定倍数。
- 最终默认5秒设置复测：实际7.069秒、309帧、帧间隔P95=34ms；4592/4592原始点采样和碰撞检查、0碰撞/0无效、20次工作台预览清理、0重载、最终关节正确。高负载会延长预览时间，未承诺固定60FPS或物理实时运行。
- Release/Debug 相关构建及各10/10回归通过，覆盖架构、碰撞工作流、场景系统、工作台生命周期、文档事务、视口呈现、Top-K分页、IK、多IK、喷涂/播放。两配置主程序 --smoke-exit-ms 2500 启动退出码0；UTF-8/CRLF及根/两个子仓git diff --check通过。新增MotionPlanningCore预览接口，无新依赖、无项目格式变化。
- 日志：外层build/playback-smooth-build-release.log、playback-smooth-build-debug.log、playback-smooth-tests-{release,debug}.log、playback-smooth-real-cdf-{batched,default}-release.log、playback-smooth-app-{release,debug}.log。最新EXE C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 2EF3F23DA09F7F1C7630E527815F4798ECB162BC3B47B46E668483C0744B6DB1；用户原运行进程未终止。


## 2026-10-02 播放进度与实际画面呈现同步

- 用户反馈上一轮播放加快后画面跟不上。复查发现控制器16ms定时器推进与RobotViewport重绘定时器独立，Qt可以合并多个update请求；旧的robotStateUpdated在render之前发出，也不能证明某个姿态已显示。仅测总耗时、界面心跳或paint次数不足以验证这个问题。
- RobotViewport新增requestFramePresentation/isFramePresented呈现票据：paintGL捕获本帧请求编号，完成场景绘制后记为rendered，QOpenGLWidget::frameSwapped后才记为presented。通过现有MotionPlanning typed port与adapter转发，不向MainWindow或领域算法添加Qt呈现语义。
- MotionPlanning控制器最多保留一个待呈现姿态；下一次定时回调先等上一票据确认，期间丢弃墙钟时间，每次最多推进1/60秒预览。首帧严格从t=0开始；末帧确认后才宣布完成、解锁结果和自动喷涂导出。单点轨迹也保持定时器直到呈现确认；手动停止/真实文档变更清除等待状态。隐藏视口或暂停绘制时不继续堆积播放进度，恢复时不追赶。
- 保留上一轮全部原始点的碰撞验证、TCP线/喷涂测量、ABB符号、多圈角度、时间轴插值和8ms批次预算。预览总时长是目标；渲染或采样过载会延长。更新播放时长提示，不更改原轨迹、导出、项目格式或依赖。
- 回归新增实际禁止viewport重绘400ms后进度/首帧保持、恢复100ms无追赶、末帧确认阻塞完成、单点轨迹完成，以及每个提交显示姿态在覆盖前确已呈现。测试末帧人工阻塞只阻塞完成确认，统计覆盖次数直接读取真实视口确认，避免把测试注入当作绘制丢帧。
- Release真实ABB4600已有4592点CDF结果，5秒目标下实测8972ms（并行构建Debug时），444次提交姿态全部呈现、覆盖未呈现姿态次数0，呈现帧间隔P95=24ms。全部4592点碰撞校验，0碰撞/0无效，20次工作台预览清理、0重载、最终关节正确。此处检验Qt呈现确认，不承诺固定显示器FPS或硬实时运行。
- Release/Debug主程序与相关回归目标构建通过，两配置各10/10相关回归通过；两配置主程序--smoke-exit-ms 2500启动退出码0。UTF-8/CRLF及根和两个子仓git diff --check通过。新增应用层typed port及RobotViewport呈现接口；不修改预编译SDK。保留全部既有未提交修改。
- 日志位于外层build/playback-present-*.log。Release产物C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 5B852FEB1BBB10ABA19CFA6E20892D014D22390B02ED7DADE31B32555977B833。


## 2026-10-03 APF/CDF 阶段检查、曲率平滑与时间质量

- 用户以 11111.txt 全逆解/全局 Top-1 为初始解，要求能查看 APF 与最终 CDF/QP 的实际结果、速度和优化质量，修复残留不平滑。
- 根因：旧 QP 一阶差分只直接惩罚路径长度、求解回退可归零平滑、整段后处理因单个受限节点最低 Phi 降低而全部撤销；原始时间可重复且结果 qd/qdd 为空，播放预览不能用来判断执行速度。
- 所有者 ProjectMotionPlanning：新增 TrajectoryQuality、三阶段领域快照、非均匀弧长二阶目标和积分权重跟踪、保留平滑的数值回退、唯一上三角五带 Hessian、6 轮多尺度碰撞/余量/走廊约束线搜索、最终时间缩放与离散动态检查。默认一轮 QP、实际碰撞检测/高密度验收、ABB 符号/turn 均保留。
- Workbench 仅投影分析数据：CDF 初始栏保持输入，新增独立阶段表/单点应用/播放/TXT、曲率和平滑参数、缺省动态限位设置、阶段质量对比/关节速度加速度/TCP 投影坐标速度/验收日志/CSV。动态播放复用既有呈现票据和原始采样入口，1× 与预览明确区分。保存的关节位置/速度/加速度同步转换到既有逆解符号。
- 实际 world TCP snapshot 后台独占调用。对照指标采用同一输入加密参考参数，不以重新定时降低速度冒充几何平滑。真实几何/文档变化清报告，运行态应用/播放与纯预览不清报告。无第三方依赖、无项目格式变化；新增领域质量类型/API 与结果字段，旧接口保留。
- 明确限制：APF/CDF 不证明非凸全局或时间最优；报告 Phi 与无碰撞区别、TCP 偏离输入、使用缺省限位的轴数；节点加速度为有限差分，分段线性折点连续物理加速度未经认证。使用说明 docs/cdf_stage_quality.md。
- 测试入口增加 --cdf-analysis 真实 GUI 全流程/三阶段导出/质量截图/应用与播放，以及 --cdf-reference 独立实际 TCP 原输入对应检查。测试文件对话框须使用 Qt 非原生形式；首轮长 GUI 回归中原输入检查漏传 robotId 导致测试失败，已纠正且 Debug 独立复验通过，最大位置误差 0.000995748 mm、姿态 0.0000474286 度。此测试问题没有改变优化输入。
- Debug 初轮相关 10/10 回归通过。完整真实轨迹平滑/质量/独立碰撞验证、最终两配置构建与启动结果待补充。

- 最终验收：真实 GUI 全流程退出 0，749 输入点/4592 加密节点的三阶段应用、导出、质量报告及实际 TCP 指标通过；APF 阶段全部 4592 采样点播放，目标 5 秒预览实测 6.880 秒，报告可停止/重启且不会因运行态单点应用而清除。修复首次长回归发现的 setPlaybackActive 只刷新 Basic 按钮、未刷新 CDF 阶段按钮/锁定状态问题；新增对应 UI 回归。非法时间阶段的动态曲线自动禁用，防止局部有限速度曲线误导；Windows 字符集不支持的平方单位显示改为 ASCII ^2；关闭旧分析窗口同步清空 QPointer，避免重新运行时复用关闭中的旧报告。
- 两次完整优化的最终 TXT SHA256 都为 500A619461BD20FA1A9B8F17829F3CFDC4F72A60B5734D4193979637D12E08F2，几何/时间完全一致。最终完整 GUI 计算 644.339 秒（期间有其他构建/测试，不当作独占速度基准）；外层 build/cdf-quality-real-gui-final.log 和 cdf-quality-results-final 保存证据。
- APF→最终关节长度 181.10235→73.664924 rad，共同输入参数弯曲代价 1.4394369→0.085887398（下降约94%）；最大折角仍154.77863度，不能宣称处处光滑。最终位置/速度/离散加速度/运动碰撞越限均0，峰值21.434217 deg/s、119.76036 deg/s^2，保守定时5463.457 s。全部4592点 TXT 独立重新导入并按线性关节插值0.001 rad复验，0碰撞段、首尾有效，日志cdf-quality-independent-check.log；第二次导出逐字节一致，复用该验证证据。
- 安全/工艺限制明确展示：Phi=-9.1901855mm，仍未满足设置的10mm安全裕量，结果标为间距未达到的部分结果；最大TCP偏移993.51619mm、姿态151.96542度。无碰撞和弯曲改善不代表保持原末端位姿或达到非凸/时间全局最优；离散动态检查不是连续物理执行认证。
- CMake配置及Release/Debug主程序、领域和相关回归目标构建通过，两配置各10/10相关回归通过；最终图表/按钮状态修正后Debug额外1/1喷涂/UI回归通过，Release重新10/10通过。最新版两配置主程序--smoke-exit-ms 2500退出0。构建期间曾因运行中的本次测试占用CoACD DLL部署失败，测试正常结束后重新完整构建成功，没有终止用户进程或修改SDK。源码UTF-8/CRLF及根/两个子仓diff检查通过。
- 最新程序 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 4E7DEE67D5AAD2ACA1D99FEDA9FC1F028EF873FECDA0484A22F5D77F02C40343。默认一轮QP保留，无新增依赖/项目持久化字段；新增领域TrajectoryQuality、阶段结果字段和可选动态参数，旧repair接口保留。

## 2026-10-04 APF 提前绕行（未解决实际 Top-1 全路径）

- 变更所有者 ProjectMotionPlanning：前方约120mm距离势场、当前位置20mm内优先真实当前位置梯度、实际FK位置投影吸引参考、平滑早期关节/腕部偏移、关节吸引的位置零空间分量、走廊内投影、距离梯度恢复间距；局部回退将安全积分曲线分配至之前4/12/32/64/128站点，逐段重新检查位置参考和碰撞，固定锚点及原点数保持。
- 安全锚点按12/40/120点及60/120/240mm扩展，遇到不安全锚点向外寻找；最后使用实际有限关节范围。终点最多500次积分，仍受每场总预算约束，以关节目标差判断停滞。100mm、实际模型/碰撞过滤、0.001rad采样、ABB符号、turn、默认一轮QP不变；无新依赖/SDK或公共API变化。
- Viewer regression新增--apf-window，真实项目/FK/检测器/原始Cartesian参考独立复现失败窗口。新单元回归检查圆障碍提前偏移和关节线性插值形成TCP大弧的非线性腕部位置校正。Release/Debug 主程序、APF/质量/Headless/Smoke构建通过；最新各10/10 CTest通过，两个主程序启动退出0，UTF8/CRLF和git diff --check通过。
- 真实11111.txt/749点Top-1/4592加密节点完整GUI实测：906.6秒（约15分7秒），仍在当前加密节点1648→1657的修复区间失败，三档安全锚点/真实限位搜索未连通。初始1602碰撞段，部分修复后1140，QP=0，正式输出=0。未处理区间仍有942.9mm参考偏差；失败阶段的平滑和偏移指标不是合格结果指标。不能宣称完整无碰撞路径、成功率提升或提速，尤其本轮失败耗时比此前322.4秒长。
- --cdf-shape回归退出0仅证明失败正确拒绝发布及诊断/导出功能，不代表求解成功。真实证据：外层build/apf-anticipatory-real-gui-absolute.log、build/apf-anticipatory-real-results-absolute及C:/b/rs105-merge/Release/bin/log/cdf/cdf_20261004_124511_927_38184.log。局部窗口亦未证明100mm内无解，纯APF为不完备搜索，不能提供任意输入必成功保证。
- 更换Top-K候选/起终逆解构型会改变当前指定的起终关节状态。已询问用户究竟固定Top-1起终关节角，还是允许保持起终TCP位姿而改变有效逆解构型；在答复前保留选定Top-1。不把更多绕行或放宽100mm当成授权。
- 当前Release程序 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 5D28D1A8BAC524545A7105CBEC5B5D2AF2FC5D2CDF01C59F8442F65BE60BD39E。保留此前全部未提交成果，本轮新增机制已构建，但用户要求的完整成功轨迹尚未实现。


## 2026-10-04 Top-1 平滑避障最终交付（以此段为准）

- 本轮已完成：ProjectMotionPlanning原始Cartesian连续IK可选种子、多尺度128→2节点平滑、实际FK边界投影、局部角度恶化保护、真实碰撞/有序100mm走廊门控、单轮QP、最终0.00025rad发布门槛。GUI新增允许等价端点构型勾选项；两端完整TCP位姿保持，中间姿态不锁。
- 最终交付外层results/11111_Top1_Smooth_100mm，源数据build/apf-smooth-top1-delivery。原Top-1的B1→B2切换改为授权的B1→B1连续实际IK，保留J6圈数。11个本轮源文件，旧调用兼容，无新依赖/SDK/项目格式修改；保留所有此前未提交成果。
- 最新独立验收使用导出TXT原始关节线性插值0.00025rad：APF162098次、最终153115次，均0碰撞/0限位；最终最大有序TCP偏移99.20085362mm，端点位置0.000694158mm、姿态0.0000302533度。最终时间、离散速度/加速度检查通过。
- 旧→新：关节长度78.183→54.994rad；TCP超过45度折角1026→65处，95%分位130.523→9.874度；最大关节折角159.713→107.002度。不是处处光滑，仍有局部折角；TCP偏移RMS58.870→66.239mm，平滑与贴近原参考存在取舍。
- 重要：build/apf-smooth-top1-run1虽然几何更平滑且0.001rad通过，但0.00025rad检出1个碰撞段，已排除，禁止重新作为无碰撞结果交付。最终安全候选两阶段在原始顺序下均通过更密验收，未通过改阈值或仅查节点制造成功。
- 最小节点间距0.253040mm，10mm安全余量仍未达到，partial/successIncludingMargin=0保留；当前仿真有限采样验收不等于严格连续碰撞证明或连续动态认证。生成1435.74秒，一轮主QP；保守执行时间5111.849秒，不宣称提速/全局或时间最优。
- 最终Release/Debug构建及各10/10相关回归通过，两配置主程序启动退出0；UTF-8/CRLF和根/两个子仓diff检查通过。日志apf-smooth-dense-gate-*及apf-smooth-{apf,final}-dense-validation.log。Release EXE：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 D0E24A2230EB341B01D2D5C0C074C10888548F930707A5AD30990F680B8D6B02。运行中的用户程序未终止；仅停止过明确属于本轮的过渡试验进程。
- 实现及结果说明docs/apf_smooth_top1.md与交付README.md；最终验收JSON/CSV/PNG/校验和在交付目录。
