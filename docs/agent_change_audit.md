# 当前变更审计

## 2026-10-28 B 功能迁入 A

- 所有者保持：规划/IK/APF/CDF/分段在 SMRobotMotionPlanning；切片/UV/标定/涂层算法在 SMRobotSpray；工作台负责交互和 PlanningDataSource 传递；默认场景持久状态在 ProjectDocument。MainWindow 注册新工作台，构建层负责兼容和运行库部署。
- 用户明确要求 APF/CDF 使用 B 接口，因此退休已替代的 10 个 A 私有旧文件；原件和所有先前未提交补丁均在外层 `output/b_to_a_20261028/before` 及同目录补丁。保留 A 独有碰撞工作台 16 文件和直接 Cartesian 导出功能；没有重置仓库或更新 SDK/许可证。
- 实测发现 Windows manifest 快速替换会出现错误 5；只对访问/共享/锁冲突重试最多约 190 ms，失败仍保留原文件并返回错误码/路径，不以先删除旧文件破坏原子性。回归保留异常诊断、取消的部分清单和工艺字段。
- 集成验证修正：测试输出路径缩短；大型 Eigen Debug 测试 `/bigobj`；A/B 碰撞结果接口合并；显示/隐藏与碰撞动作限制分别验证；旧 ABB 回归使用独立场景。次数预算回归和超时回归分开，不放松生产安全门限。
- 最终 Release 68/68、Debug 68/68（66 项 + 2 项独立复测）和两配置主程序启动通过。GPU 搜索/IK 两配置实际运行；GPU 膜厚、标定、UV 展开/轨迹/PAQ 和保存重开均已通过。完整排序清单、SHA256 和结果见迁移报告。

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


## 2026-10-05 TCP 扫描线越线修复与原始/旧/新分图交付

- 基线 main/6489cc4，开始时根仓与规划子仓干净。6个源/头文件变更（领域5、实际回归适配器1），另更新snapshot/audit并新增设计说明；未改GUI、SDK、碰撞过滤、公开API、项目格式或依赖。
- 根因：关节chord平滑可在FK中产生大弓线，100mm逐站球形走廊不阻止相邻扫描线越线。领域新增沿原始有序弧长的余弦核圆角参考、实际FK最小范数投影、碰撞限制的恢复幅度、物理弧长五次平滑过渡及整条候选原子提交；随后多尺度关节平滑投影到恢复后的TCP位置（节点0.201mm容差），每个窗口仍需原检测器与原走廊验收。
- 已接入APF阶段快照前与最终CDF输出前。碰撞连接与窗口验收在新增处理采用最多0.00025rad；原始100mm、合法turn、完整起终TCP、默认一轮主QP及旧接口保留。中间姿态仍不锁。恢复失败保留之前路径并诊断，不通过放宽阈值或仅修改图像伪造改善。
- 完整原Top-1/11111输入运行2025.22秒（33分45秒），QP rounds=1，两个tcp_shape_restored阶段均通过。源输出外层build/apf-shape-top1-delivery，交付results/11111_Top1_Shape_100mm；上一轮11111_Top1_Smooth_100mm未覆盖且SHA256校验一致。
- 完整最终4528点独立原始关节线性插值0.00025rad：138362样本，0碰撞/0限位；APF148005样本，0碰撞/0限位。最大有序TCP偏移最终87.664411mm、APF73.807870mm；完整端点最大位置0.000694158mm、姿态0.0000302533度。最终速度/离散加速度越限0，峰值43.858199deg/s和119.760360deg/s²，保守执行时间2039.828465秒。
- 旧→新：新增扫描线对XY投影交叉55→7，TCP>45度折角65→11、>90度21→0，最大TCP折角145.606→89.587度；有序TCP偏移RMS66.239→19.190mm，关节长度54.994→50.764rad，共同原TCP参数弯曲3.20150→1.32979。P95折角9.874→9.916度基本持平；TCP长度31.627→35.372m，因恢复扫描覆盖而增加，不宣称所有指标改善或全局最优。
- 原参考自身88处XY投影交叉；新结果总73处。新增计数基于离线主X扫描回转分组（30行），不是领域算法的硬编码约束或拓扑证明。剩余7处对应三维间距66.664～175.860mm，部分位于分组边界附近。全部三维/XY/XZ/YZ原始/旧/新独立PNG已生成、坐标一致，主要图视觉核对通过。
- 安全与性能限制：最小最终节点间距0.268106mm，10mm裕量仍未达到，successIncludingMargin=0保留；仍有11处较大离散TCP折角，不能称连续C2光滑或连续物理认证。本次33分45秒比上轮23分56秒增加，不宣称提速。
- 验证：Release/Debug主程序和相关目标构建成功，初轮各10/10；最终健壮性小修后Debug3/3、Release10/10通过，两配置主程序启动退出0。完整运行后仅修复诊断文字、距离失败出口及诊断检查点采样设置，不改变有效输入的几何步骤。最终UTF8/CRLF、根仓与规划子仓git diff --check通过。
- 日志：外层build/shape-{final-build,tests,final-tests,app}-{release,debug}.log及apf-shape-top1-delivery.log；原始JSON/CSV、README、交付metadata和checksums在结果目录。运行中的用户进程未终止。
- 最终TXT SHA256：51DCBD5666750CEAA8730CDFD07C1D8BE9BD7B85430931605415A1117302A047。Release EXE C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256：672D141A2CF4996232E346341D4FA06CC5D8536A8EDEC1553CC4EA71935041B3。


## 2026-10-08 原主窗口动态播放卡顿：语言重翻译与呈现同步

- 用户明确主视口连续碰撞未开启，要求在原主程序窗口验证。本轮仅使用现成4528点 results/11111_Top1_Shape_100mm/final_joint_trajectory.txt，不重跑APF/QP。根仓基线main/bf1c0ea；修改Shared语言服务和MotionPlanning Workbench控制器，原界面布局不变。
- 根因证据：QApplication通知诊断显示QEvent::LayoutRequest(76)在多层滚动容器/分页中每次耗时85～99ms。全局RobotQtViewerLocalizationService递归扫描重复子树，对QTableWidget每个数值/关节向量重新匹配所有翻译模板；播放中的喷涂读数更新会持续触发此路径。场景paintGL通常不足1ms，不能据此认为完整窗口无卡顿。临时notify诊断代码已撤回，原日志playback-main-notify2.log保留。
- 语言服务：缓存翻译命中和未命中结果，QCache按UTF-16文本与固定开销近似计费，容量8MiB。表格仅在model的数据/表头/行列插入/reset变化后重新扫描；析构清指针，切语言或重载词库清文本与表格缓存。LayoutRequest仍处理动态标签及其他UI，保留原中英文翻译能力；不改数值、关节符号、turn或轨迹内容。
- 播放控制器：继续复用既有呈现票据，上一姿态frameSwapped确认后才发布运行态通知和喷涂读数；原始逐点碰撞/测量/轨迹记录、8ms批次预算、末帧确认和已有插值保持。修复没有放松碰撞检测或通过丢点伪造加速。
- 新增regression/PlaybackWindowProfile.{h,cpp}及主程序可选参数，直接操作原MainWindow内的Motion Planning导入/播放控件；保持真实模型、原面板和渲染。测试输入为同一最终TXT转成既有generic JSON（角度转rad，Joint1～Joint6），未导入额外Cartesian覆盖层；保留末端线与喷涂读数，主视口连续碰撞关闭但播放独立逐源点校验保持。无新SDK/public module API、项目格式或第三方依赖。
- 原窗口Release证据：playback-main-baseline.json在120013ms超时，帧间隔P95=706ms、最大1599ms；首次修复完整18181ms、P95=45ms。最终playback-main-delivery-release.json：19637ms完成、473帧、P95=46ms、最大49ms，UI心跳P95=44ms、最大54ms；allPointsChecked=true，4528/4528、0碰撞、0无效，截图同名.json.png。耗时从播放按钮返回后计起，不包含导入或创建独立碰撞场景；5秒预览目标在负载下延长，不承诺60FPS或硬实时，不把120秒未完成当成完整基线耗时。
- Release/Debug主程序与Localization回归目标构建成功；相关Localization/Architecture/WorkbenchLifecycle/DocumentTransaction/ViewportPresentation回归各5/5通过。新增1000行表格测试覆盖数值不改、重复翻译、动态文本/tooltip、插入行、语言切换、同一语言下词库重载使未命中缓存和表格缓存失效；最后加强的reload测试Release额外1/1通过。性能结论仅来自上述原主窗口，未以简化窗口结果代替。
- 日志位于外层build/playback-main-{final-build-release,final-app-build-release,final-build-debug,tests-release,tests-debug,localization-release,delivery-release}.log；最终源文件UTF-8/CRLF、根仓/Workbench子仓git diff --check通过。最终轨迹TXT SHA256仍为51DCBD5666750CEAA8730CDFD07C1D8BE9BD7B85430931605415A1117302A047。
- Release交付为C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 AA8A704D4E4A57586E0F6EF4D66EACB3EF55A2BA6051F824085DA7FA5F035806。外层build/Release/bin中的另一份旧程序未覆盖，运行时必须使用本次交付路径。未终止用户进程，只清理本轮自行创建的诊断进程。

- 最后补充验证：两配置主程序正常启动 --smoke-exit-ms 1500 均退出0（playback-main-smoke-{release,debug}.log）。Debug原主窗口全轨迹在120142ms触发诊断超时，615帧、P95=233ms、finished=false，未取得完整源点验收；见playback-main-delivery-debug.{log,json}。Debug性能仍较慢，不能把编译/常规回归通过表述为Debug完整播放通过，本轮完整播放与提速结论仅适用于Release。测试进程均已自行退出。


## 2026-10-08 CDF RAPID MOD 按钮与工具定义导出

- 用户提供MainModule.mod，要求CDF页新增该格式导出；明确选择当前喷枪TCP并附工具定义。基线main/1ad4f8a，相关仓库初始干净。
- 新增ProjectMotionPlanning/RapidTrajectoryExport领域API，读取独立实际模型快照和当前TCP FK，按既有stored-IK→runtime符号映射处理全部六轴源点；验证串联顺序、有限数据、SE(3)、固定法兰→TCP。世界坐标m→mm、单位四元数wxyz及同半球连续处理；不抽点、不移动实时机器人，不改变APF/QP或关节数据。
- 输出MODULE/CONST robtarget/PROC main/MoveL v50 z10，新增PERS tooldata tCdfSpray和显式WObj:=wobj0。robconf沿用附件全零占位；工具负载未知，tload零负载占位。UI/文件/使用说明明确未标定字段、场景世界坐标映射和MoveL/z10不等价于原关节插值；没有使用B1～B8伪造ABB confdata，也没有宣称控制器路径已无碰撞。
- Workbench增加“导出 CDF RAPID 程序（.mod）...”按钮，最终CDF计划可用且未播放时启用；通过typed signal/controller调用领域导出并QSaveFile原子保存；原TXT导出保留。程序内容在保存对话框前冻结，避免modal事件修改来源造成导出混合状态。
- 新增回归覆盖实际ABB模型、平移/旋转基座、已知非零工具、mm与wxyz往返、符号/多圈、全部点、末行NaN、非刚性TCP、缺失实际FK拒绝。Release/Debug主程序及导出测试构建成功，各2/2（导出与架构边界）通过。
- 原主窗口导出：复用现成4528点最终轨迹，通过正式导入与CDF新按钮保存，Release/Debug均退出0；全部4528点从MOD读取后逐点对比实际viewport FK，最大位置8.296467083015937e-10 m，最大姿态1.7968547110936603e-9 rad。未重跑APF/QP；截图cdf-rapid-main-release.json.png核对新按钮可见。
- 原窗口诊断沿用--profile-playback，新增仅显式启用的--profile-rapid-export保存检查分支。首轮该诊断直接包含领域头导致主程序编译缺少include依赖，已改为独立读取MOD并调用既有viewport FK，未扩大生产主程序依赖；最终构建通过。
- 文件位置：外层results/CDF_RAPID_Export/MainModule.mod和README.md；983370字节。使用说明docs/cdf_rapid_export.md。日志外层build/cdf-rapid-{configure,build-release,build-debug,tests-release,tests-debug,main-release,main-debug}.log，两个原窗口数值报告同名.json。
- 交付EXE C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256 82FD8A3DD49BB47C3B118136D4458124E330F00D45B6AA2AAB1B3685263E939B。用户正在运行外层build/Release/bin的旧EXE（PID12644），未终止或覆盖该进程。源文件UTF-8/CRLF和三个仓库diff检查通过；无项目格式、SDK或第三方依赖变化。未做ABB RobotStudio编译/实机验证。


## 2026-10-08 tool0、12列轨迹导入、0.5mm间距与等价查询优化

- 保留上轮未提交的RAPID导出改动；领域为ProjectMotionPlanning，Workbench仅默认值/提示/控件标识，主程序诊断仍走原MainWindow。无SDK、依赖或项目schema变化。
- MOD按桌面MainModule.mod：tool0\WObj:=wobj0，无tCdfSpray/tooldata；目标仍为原TCP FK坐标，不暗中换算法兰补偿值。文件头/布局/MoveL参数与参考匹配，UI和说明已同步。
- 12列xyz/wxyz/speed/acceleration/time/type/standoff的TXT不再落入每4行拼矩阵的宽松解析；m与m/s换算、显式零/重复时间、四元数归一化，异常行整份拒绝。加速度/点类型/喷距为已识别但未接入执行的工艺列，导入提示明确；不改预编译TimedCartesianPoint。
- 安全距离领域/UI改为0.0005m，步长0.0001m；移除APF连接的隐藏5mm下限覆盖。保留排斥影响范围与原局部修复额外余量，未将“无碰撞”冒充“满足安全余量”。
- 查询提速：独立场景持久工作线程、同步批次屏障/异常回收；平滑间距验收一旦必败，停止未开始的距离任务。全值查询/最终高密度复验不变；未改QP轮数、轨迹点、浮点公式、随机搜索、关节符号/turn或碰撞过滤。
- Release/Debug原主窗口：附件234点全部导入，位置最大显示误差2.22e-16m、时间误差0，已核对球点显示。tool0导出4528点，实际TCP FK往返最大8.30e-10m、1.80e-9rad；所有MoveL工具、模板头尾匹配。两配置最终主程序启动退出0。
- 同0.5mm的600点已无碰撞输入片段、一轮QP对照：64.3719→63.7109秒，1199点TXT逐字节相同，SHA256=54ac6ab57b3bbc4b6db5ec60fd1b572011de870cfc285db7b54e6793bdacdb9a；独立0.001rad复验均0碰撞，最小节点Phi=0.127484mm。约1%的耗时差不能支持大幅提速结论，也不代表含TCP形状约束的完整Top-1运行。
- 仅线程复用的103点片段12.2408→12.4177秒、205点输出相同，无可确认加速。旧headless缺少GUI校准TCP形状/连续IK入口，直接Top-1及100点种子的试跑APF失败，没有把这些失败运行当作结果交付或性能证据。
- 初轮Release构建遇本轮基准进程占用lib_coacd.dll的部署冲突；基准退出后重建通过，未终止用户旧主程序。Debug第一版重复线程测试包含重复大圈运动检查，主动停止并缩短其同步压力测试路径；原逐轴turn/运动测试仍保留，随后重建并重新运行正式回归。
- 用户追加GPU询问：重新读取升级后SDK，底层CollisionWorld已有ICollisionBackend注入，但正式CollisionScene/ProjectCollisionRuntime没有公开注入点，只有Default/Fcl/Coal；没有现成GPU实现或发现nvcc。GPU距离场近似会改变梯度/轨迹，CPU复验只能保留安全验收，不能保证同结果；本轮未安装CUDA或伪装GPU开关。
- Release/Debug最终均9/9回归通过（23.67秒/604.32秒）；Debug查询等价测试347.98秒，已完整结束。详情docs/cdf_import_tool0_performance.md；日志外层build/tool0-import-*、process-pose-main-*、tool0-main-*与margin05-*。
- 交付EXE：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe，SHA256=9BF925C91FDB5A07287AC3750342B745FD154601ECA5AD17061EF0FC07BEDA9C。源文件UTF-8/CRLF与根仓/两个子仓diff检查通过。

- 用户明确GPU不可行就不强用；本轮最终保持CPU方案。所有本轮测试/基准进程均已结束，未中止用户旧程序。

## 2026-10-08 恢复10mm与CDF候选碰撞精度修复

- 最新用户明确安全距离恢复10mm，并指定桌面11111.txt为复现输入。此前查到的175327日志实际为234点trajectory.txt，不能代表749点11111；停止本轮自己启动的234点原窗口诊断PID3088，改跑11111。
- 已知失败原因：APF/QP/平滑候选以0.001rad验收，最后0.00025rad在7段发现碰撞；最终拒绝是正确行为。修复归ProjectMotionPlanning，将所有候选接受统一到最终步长上限0.00025rad，保留最终绕缓存的复验；形状恢复失败时保留的前序路径现在也通过同精度验收。未放宽检测阈值/过滤规则、未修改SDK或机器人符号/turn。
- 领域选项、Widget设置结构和CDF控件默认恢复0.01m，UI步长恢复0.001m。APF与QP读取同一设置；有限差分5e-4rad未误改。主QP仍一轮，TCP走廊仍100mm。导入12列/原矩阵格式和tool0 MOD功能保留。
- 原MainWindow诊断新增--profile-cdf-top1，依次调用真实导入/全逆解/全局Top1/CDF按钮和阶段导出；不在MainWindow新增领域算法。不使用简化窗口替代原窗口测试。全逆解默认设置对话框由诊断显式接受。
- Release相关8/8、Debug相关6/6回归通过（25.45秒/548.91秒），Debug正常主程序启动退出0。Debug最后一次仅诊断适配器重建曾因本轮测试进程占用lib_coacd-D.dll导致部署失败；等测试结束后重建成功，没有关闭用户进程。
- 本次完整原窗口日志cdf_20261008_194129_429_42720：11111/749点/全局Top1 cost=148.219446482，safety=0.01、QP rounds=1、统一0.00025rad，生成4528点连续IK参考；APF扫描96个碰撞/形状区间。完整运行已结束，输出4528点，最终0碰撞段；未达到10mm余量，partial状态保留，详见后续记录。
- 部署核对：短构建目录和用户实际外层build/Release/bin中的42个DLL逐文件SHA256一致，项目ABB4600-burnner.sys.json也一致。计划在完整验证后备份并同步EXE，避免用户继续启动0.5mm程序。

- 完整原MainWindow运行结束：1998.876秒，最终4528点，两个tcp_shape_restored均完成，主QP一轮，final gate绕缓存0.00025rad通过；界面保留输入并启用最终导出，诊断退出0。最小最终节点Phi=-9.2761862mm，即距离约0.7238138mm；successIncludingMargin=0，不宣称达到10mm。
- 独立导出复验：最终137279、APF148061个原始线性插值状态，均0碰撞/0关节限位；最终最大有序TCP偏移83.84917057mm，APF73.80788734mm，起终位置0.000691112mm/姿态0.0000303233度。最终速度/离散加速度越限0，执行时间2039.828465秒。使用原MainWindow产物；独立验证工具仅读取文件与创建实际模型场景，未以简化测试窗口生成替代轨迹。
- 对应参考核对：此前与本次4528点加密连续IK参考差异至多5.01e-7度/5.01e-7秒，符合当前UI导出精度；仅用已核对的连续种子重建独立验算的有序参考，不使用旧最终轨迹。第一次独立工具缺少SMROBOT_DATA_ROOT导致找不到URDF，指定源码data目录后两次复验都完整退出0。
- 新结果在外层results/11111_Top1_10mm_20261008，含阶段TXT、独立验收JSON/TCP CSV、原窗口截图/报告、日志、metadata、README和SHA256。没有覆盖旧结果。
- 交付EXE SHA256 AF3BCDDE3A56E7229B0938AC9024DBA7DE11350AB606659CE918161B6CEF67D3。已备份外层build/Release/bin旧EXE到build/backups/cdf-before-10mm-20261008，并同步本次已验证EXE；部署路径启动退出0。C:/b/rs105-merge/Release/bin保留同一版本。用户进程未终止；本轮验证进程均已结束。

## 2026-10-11 主视图碰撞距离显示与用户新轨迹复验

- 基线 main/33fac4a，初始根仓干净；用户确认主视图距离线/数字丢失。ProjectScene 每帧调用 SDK Continuous，CDF 返回 PendingManualRefresh；独立播放快照仍只做逐源点布尔碰撞验收，不产生主视图最近点。
- 最小修复在 RobotViewerCore/ProjectScene：主检测开关开启、检测器有效且启用、显示可见且请求距离/最近点时，每个实际渲染姿态以 ManualNearest 做一次完整查询。隐藏距离仍走 Continuous，关闭主检测不执行查询。未改变 SDK、持久化、公开 API、碰撞过滤、APF/CDF、关节符号/turn、播放轨迹或逐源点检查。
- 原 MainWindow 诊断增加 --profile-collision-distance，调用原 Collision Detection QAction，记录CDF最近点状态、距离变化及提交的距离线。4528点基线：475帧、nearestFrames=0、PendingManualRefresh、19.293秒；修复：476帧均有最近点/overlay，0丢失、24.469秒、P95=58ms（基线49ms）、4528点全检查0碰撞/0无效。显式距离查询有额外成本，未宣称无开销/60FPS。详见外层 build/distance-main-{baseline,fixed}-visible.json。
- 原窗口附加测试用户2050点时出现呈现等待，120秒只收到180帧但所收帧全部有CDF距离；后续另一次只收到2帧，主动通过原Stop按钮结束。本轮没有完成该文件的完整GUI播放验收，不用这些运行宣称通过。诊断进程全部结束，未停止用户进程。完整距离回归依据上述4528点原窗口对照；用户新文件的全程碰撞结论依据下面独立密采样。
- 用户追加检查新轨迹：第一次给出的桌面 trajectory.txt 为4×4末端位姿矩阵，未据此重求逆解冒充结果；更正文件 ABB4600_urdf_apf_cdf_qp_trajectory.txt 为2050点、888.298362秒，SHA256=8f885a6daae4b754d9aff7f8588493636c0a85e6e94500e2c83527bd2196697c。
- 现有Headless --cdf-inspect --linear-input 按原多圈角直线插值、原ABB符号、实际ABB4600/burnner模型检查：0.00025rad的117889个采样位置通过；0.0001rad检查全部2049段，检出1段state_in_collision。诊断工具现会打印首个无效采样、原段号及时间，不改变采样或检测算法。
- 问题是原点1178→1179（516.229036→516.737332秒），51/189处，即516.366195238秒。导出的中间姿态重新建立独立场景、恢复与用户EXE一致的DLL后仍state_in_collision，起终相同见证姿态都无效。属于CDF机器人—工件检测器碰撞，非限位；并非完全无碰撞。另default_collision SceneAll报告相邻Link5/Link6接触，未隐藏/修改其过滤。
- 报告与最小复现：外层 results/trajectory_check_20261011，含validation.json、README、segment_1178_1179.txt、collision_witness.txt和日志。仅核验原文件，未重新优化或覆盖原始轨迹；有限采样通过不等价于连续无碰撞证明。
- Release/Debug主程序构建通过；两配置场景系统、语言缓存、主题和架构回归各4/4通过。Headless诊断两配置构建通过，负例返回1为预期；Debug有预编译OMPL PDB缺失的LNK4099告警。一次Release Headless部署被本轮原窗口占用lib_coacd/tbb阻止，结束本轮进程后重建成功；其部署会覆盖这两个DLL，已恢复用户原程序同版DLL并复验碰撞见证。未交付这两项依赖变动。
- 用户EXE先备份到外层build/backups/distance-before-fix-20261011，再同步到build/Release/bin；短构建目录C:/b/rs105-merge/Release/bin同版SHA256=B81B637944F7B5044D30D92DAFB0785EAEF51D72ED45E16DF72C72DA2DD21C44。用户部署入口 --smoke-exit-ms 1500 退出0。源文件UTF-8/CRLF、根仓和SMRobotMotionPlanning子仓diff检查通过；原用户轨迹SHA未变。


## 2026-10-12 Trajectory Transform 与逐点最近距离图

- 领域新增 ProjectTrajectoryControlPointEditor::transformCartesianTrajectory / serializeCartesianMatrices、MotionPlanningProjectStore::removePlan 和 ProjectPlanningSceneSnapshot::nearestDistance。Qt负责输入、选中、后台任务和原子保存；文档修改统一mutateProject，SDK及APF/CDF算法、碰撞过滤未变，无新依赖。
- Motion Planning第三页Trajectory Transform，与Basic共享选中的Cartesian计划，支持两种格式导入后自动显示点、删除、整体变换和矩阵TXT导出。变换T'=A*T，R=Rz*Ry*Rx，绕世界原点依次固定X/Y/Z旋转、最后沿世界轴平移；输入mm/deg，内部m/rad。保留点顺序、时间和已有速度字段，清除旧关节解/多IK/图及关联CDF初始解。导出为四行4x4（平移mm）+秒时间+空行；不包含12列工艺字段。窄侧栏允许横向滚动，避免按钮/文字右侧永久裁切。
- 验证发现原parseGenericMatrixText把矩阵后的时间戳归给下一点，首点/第二点重复0且末点时间丢失。现在按首个数据行识别前置或后置时间，保留无时间矩阵格式；增加两种时间布局、单位、非交换旋转、位置/姿态一致变换、元数据、旧IK失效、失败不部分提交、矩阵往返和精准删除测试。未重新计算或覆盖历史用户轨迹。
- Basic新增“绘制最近距离变化图”。动态播放保存每个实际处理的原始点runtime角度及本次独立碰撞快照；停止后按钮在后台逐点查距（可取消），不将精确距离查询加入播放关键路径。检测范围沿用播放检测器，图/文件明确检测器id、轨迹名、快照时间、完整/部分点数。碰撞=0mm非负几何间距而非穿透深度；无效=NaN/折线断开，任何检测器失败不冒充有效最小值。弹窗可导出TXT，含1起点序号、原时间、mm距离、有效/碰撞状态、检测器和runtime关节角。保留turn与既有ABB符号映射。查询并非渲染帧抽样，也不是段间连续无碰撞认证。
- Release/Debug主程序及ProcessPoseImportTests、Headless构建通过；两配置相关4/4回归通过（导入、导入/变换、架构边界、语言服务）。实际附件234点和11111矩阵749点的所有点变换/导出再导入均通过。Debug存在原预编译OMPL的LNK4099 PDB告警。
- 原MainWindow诊断新增显式--profile-trajectory-transform和--profile-nearest-distance-plot，未用简化窗口替代。Release两格式导入/变换/导出/删除通过，位置最大误差2.32e-15m、旋转矩阵1.19e-15、时间误差0；Debug附件234点同样通过。实际CDF前20点播放两配置均完成、20/20距离有效、TXT行数一致、图窗截图通过。Release9.742秒/P95=35ms，Debug31.711秒/P95=130ms（5秒预览目标）；不据此宣称长轨迹加速或Debug流畅性。已知碰撞见证在新最近距离API下正确为0/碰撞=1，非有限和错误维度拒绝/NaN。
- 证据：外层build/trajectory-tools-20261012，含原窗口JSON、截图、导出矩阵/距离TXT、碰撞见证日志；构建/ctest日志在C:/b/rs105-merge/trajectory-tools-*.log。UTF-8/CRLF及各仓diff检查通过。
- 用户旧EXE备份在外层build/backups/trajectory-tools-before-20261012，新Release同步外层build/Release/bin及C:/b/rs105-merge/Release/bin。SHA256=034CABF644137E43A86CA01107924AE471FBA77199CFA78333ACA194980773D9。Release全部SDK DLL与用户入口核对一致，未交付依赖替换；未终止用户进程。


## 2026-10-12 最近距离图零刻度线

- 仅调整MotionPlanningEditorWidget.cpp的绘图：最近距离图纵轴始终包含0，加粗高对比度虚线及“0 mm”标签，避让临近普通刻度；深浅主题使用不同对比色。喷涂曲线与距离查询/数据导出不变。
- Release/Debug构建通过；原MainWindow 20点完整播放、距离查询与TXT导出通过，截图nearest-zero-release.json.distances.png确认零线清晰可见。证据在外层build/trajectory-tools-20261012；UTF-8/CRLF、diff检查通过。
- 用户旧程序PID49956仍在运行，未终止或覆盖；新版同时位于C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewer_ZeroLinerx64.exe，SHA256=E2B8304D3FDB90FA83B1B56E851FA7934F35A59B914C127502079E3FF9517763。

## 2026-10-12 CDF FANUC LS 导出

- 完成：CDF 新增“导出发那科机器人程序（.LS）...”按钮，全部最终关节点经独立实际 TCP FK 转为 XYZ/WPR；模板 UF2/UT1、400mm/sec、CNT100、N U T/零圈数，ASCII/CRLF/原子保存。新增领域 API，未增加依赖或修改原优化/导出语义。
- Release/Debug 主程序和新回归目标构建通过，两配置各 3/3 导出/导入回归通过。正常启动原 MainWindow 的 CDF 可见按钮导出两配置均通过：4528/4528 点，位置最大误差 8.35237e-7 m、姿态 1.81050e-5 rad（三位小数序列化）；截图与 JSON 为外层 build/fanuc-main-visible-{release,debug}.*。
- 原诊断脚本可能点击隐藏页面；本轮诊断增加真实工作台 action 和按钮可见性核验。显式 --profile-project 路径下工作台动作禁用，最终改用正常启动加载已有 ABB 项目的原窗口验证，没有改变产品工作台切换逻辑。
- 发布至 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 与外层 build/Release/bin/RobotQtViewerrx64.exe，旧 EXE 保存在 build/backups/fanuc-export-20261012；两入口 SHA256=8C783D19E6630D737689F74AA1285E34D4CA774559315BF18137AEED39AAB42B。桌面 JHA.LS 原模板未修改。
- 使用限制：当前为笛卡尔目标格式导出，FANUC CONFIG/turn 沿用模板而非机器人逆解；UF2/UT1 需匹配实际场景，控制器 L/CNT 路径与原 CDF 关节插值不同。未宣称 FANUC 编译或实机无碰撞已验证，详见 docs/cdf_fanuc_export.md。


## 2026-10-12 Project Assembly / Move Base 无响应修复

- 根因：装配工作台的 panel resolver 随选择在 statusPanel/taskWidget 间切换；初始化只注册 statusPanel，后来 setCurrentWidget(taskWidget) 报 not contained in stack。未纳入布局的子控件仍可能 isVisible=true，不能据此判定可操作。此外 MoveRobotBase/MoveSceneObject 未进入显式打开/置前任务 dock 的菜单分支。
- MainWindow::updateTaskPanel 在选择前按需注册 resolver 返回的新页面；Move Base/Move Object 复用已有任务面板显示入口。SceneExplorerWorkbenchLifecycle 创建备用任务页后先隐藏，避免未注册时覆盖状态页。变换草稿、Apply 的 SceneEntityWorkflow、文档 dirty、预览和 Cancel 机制不变；无依赖或领域 API 改动。
- 新增显式 --profile-move-base <report.json> 原 MainWindow 诊断（沿用 PlaybackWindowProfile 文件），直接触发真实树右键菜单，不使用简化窗口。断言必须是 QStackedWidget 已注册且当前页面，并检查几何尺寸和可见性；覆盖未选中、右键、隐藏 dock 恢复、100mm 预览/取消/Apply/恢复、Motion Planning 往返、burnner 和 ABB 切换。测试不保存项目文件。
- Release/Debug 主程序构建通过；两配置原窗口各 17/17 检查通过，截图确认编辑器完整布局，日志无 not contained in stack。两配置架构边界、场景树投影、工作台生命周期、文档事务、插件回归各 5/5 通过。
- 证据：外层 build/move-base-{baseline,release,debug}.json、.json.png、.log、.err.log；复现脚本 build/test_move_base.ps1；构建及 ctest 日志在 C:/b/rs105-merge/move-base-*.log。
- 已同步 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 与外层 build/Release/bin/RobotQtViewerrx64.exe；SHA256=8FA8DA16A2981930C8278062263D20C1E2983549A5F3DB78041935A38BD5C9B3。旧 EXE 保存在外层 build/backups/move-base-before-20261012，未终止用户进程。原有 FANUC/CDF/轨迹等未提交修改均保留。

## 2026-10-12 外部 Freeform APF/QP 自动流程交付

- 外部目录 C:/Users/14390/Desktop/Freeform_APF_QP，入口run.cmd。复用实际ProjectScene、全逆解、分层图Top-30/Top-1和原APF/一轮CDF-QP；本轮未改主项目业务源码或SDK，新增外部C++入口与Python编排/配准/图表/原主窗口录像。依赖当前源码、SDK和已有Python科学计算环境，不是独立便携软件包。
- 姿态按最新用户指示工具局部Z轴±90度对比：−90为2400/2400、各8解，+90为0/2400。工件按0.001缩放，约平移(1.2,.6,.6)m、RPY(-90,0,-90)度；2000曲面点间距约1mm，400引入引出点约100mm，全部保留。
- 原APF/QP输出13770点，未达10mm。用户明确只要求无碰撞。独立0.0001rad采样曾检出7个碰撞状态，故未直接交付原stage_2。新增外部DenseRepair：保守URDF力臂界及包围球一阶速度/Taylor余项界，实际FK导数有限差分误差4.94e-11m；自适应距离检查发现13段需修复，全部小幅正弦平方微扰成功，影响91点、最大0.00572958度。完整自适应复验通过，保守间距下界0.0100098mm。
- 最终2556864个独立密采样状态、0碰撞；项目模型限位/速度/离散加速度违规均0，起终TCP误差1.57e-9m/1.01e-9rad。节点最小间距0.596690mm（不是整段最小距离），10mm软目标未达且未伪报成功。最终时长4289.127s。中间TCP最大位置偏移99.3348mm、法向偏差68.3798度：当前未约束中间完整位姿，不能称为保法向工艺轨迹。
- FANUC导出13770点逐点反序列化，XYZ误差≤8.40e-7m、WPR误差≤1.85e-5rad；播放JSON与stored-sign TXT一致。UF2/UT1、400mm/s、CNT100、CONFIG沿用原模板，未做控制器编译或其L/CNT插补碰撞复验。
- 原MainWindow完整动态播放13770/13770点，无碰撞或无效状态；13563ms、P95帧49ms。MP4录自该窗口，取帧核对真实机器人/工件/轨迹，滚轮仅缩小展示视图，不改几何。原桌面输入哈希未变。Release/Debug入口均构建通过，实际Top-1 FK输出哈希一致；Debug仅做FK验证，未重复昂贵规划。Debug原OMPL PDB告警保留。
- CMake外部目标临时钩子已清除，原GUI SHA256=8FA8DA16A2981930C8278062263D20C1E2983549A5F3DB78041935A38BD5C9B3。completed.json绑定输入/模型/算法及输出哈希，run.py --no-view --no-record实测复用通过。之前未提交的产品修改全部保留。
- 主要证据：output/final/{adaptive_motion_validation.json,validation.json,export_validation.json,shape_validation.json,main_window_playback.json,robot_animation.mp4}，output/{polish.log,cache_replay.log,source_manifest.json}。失败旧验收存validation_before_dense_repair.json；stage_*.txt仅供对照，交付使用final_joint_angles.txt。


## 2026-10-13 轨迹点姿态预览（进行中）

- 用户优先要求 Basic Planning 新增“显示轨迹点姿态”。沿用会话控制器和 typed viewport port：widget发意图、controller保存会话开关、ProjectScene接收完整位姿、私有overlay pass批量绘制RGB局部XYZ细线。默认关闭，独立于球状点显示；轴长按范围限定15至40mm。
- 不改原轨迹/逆解/优化，不持久化临时预览；追加showOrientations默认false兼容现有调用。基于SDK RenderCore Geometry(GL_LINES)缓存一次上传/一次draw，避免数千debug.drawLine的分配和draw开销。
- 外部Freeform规划按用户要求保留机器人基座，暂不继续失败规划；最新同名轨迹为世界坐标矩阵格式，需要另行输出不可达点报告。本轮先完成主项目姿态显示。
- 验证：Release/Debug主程序构建，原MainWindow导入矩阵和四元数轨迹、显示/隐藏/球点独立性及大量点预览。旧EXE/SDK已备份于外层build/backups/trajectory-orientations-20261013。


- 本轮姿态显示已完成：Basic Planning 的“显示轨迹点姿态”默认关闭，RGB对应完整位姿的局部XYZ；15~40mm自适应轴长，1像素细线，独立于球状点开关。开关仅更新覆盖层，不重建轨迹表；轨迹替换/变换/删除走现有refresh通知，ProjectScene统一管理预览并缓存单个GL_LINES Geometry。私有pass未加入安装头文件，SDK/DLL未变；应用层typed viewport port追加默认false参数，旧源码调用保持默认行为。
- Release/Debug主程序构建通过，Release兼容回归RobotQtViewerConfigurationTabsSmoke 1/1通过；原MainWindow实测最新矩阵7056点（两配置）及12列四元数7056点（Release），三项均RGB显示通过、关球点姿态独立显示通过、取消后framebuffer逐像素恢复通过。测试禁用诊断窗口鼠标输入避免用户操作干扰，不使用简化窗口。证据在外层build/orientations-{matrix-release,quaternion-release,matrix-debug}.json及截图/日志。
- 编译部署覆盖过的Release lib_coacd.dll/tbb12.dll已从本轮备份恢复，Debug未发现部署差异。CMAKE_PROJECT_INCLUDE外部钩子已清除。用户正在运行外层build/Release/bin/RobotQtViewerrx64.exe（旧版），保留不终止/覆盖；新版在C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewer_TrajectoryAxesrx64.exe，SHA256=3898485754431EF1380822AC0F23F01D57CD1E24D0D81F20F9DDA03CCA701465。交付清单为外层build/trajectory-orientations-delivery.json。


## 2026-10-13 轨迹点局部轴姿态旋转（已完成）

- 在现有未提交姿态轴预览改动上继续，保留原世界系整体变换。在Trajectory Transform的“变换轨迹”对话框增加模式选择及局部XYZ轴、角度字段。位置保持不变，使用R_i右乘所选轴旋转；时间/工艺属性不变，旧IK及图结果按原领域规则失效。
- 所有者为ProjectMotionPlanning/TrajectoryControlPointEditing，新增typed轴枚举及旋转API；Widget只收集草稿，controller通过commitMotionPlanUpdate提交并通知原轨迹视图/姿态覆盖层。无SDK、依赖或项目格式变更。
- 验证领域局部/世界旋转区别、局部轴不变、元数据和错误原子性；Release/Debug构建及原MainWindow导入-局部旋转-导出回读（XYZ、正负角及累计旋转），保留旧整体变换回归。备份在外层build/backups/trajectory-local-rotation-20261013。

- 本轮局部轴旋转已完成：Trajectory Transform → 变换轨迹 → 只旋转姿态（各点局部坐标），可选红/绿/蓝对应的X/Y/Z及正负角度。使用R_i * R_axis(theta)，各点位置保持原值；时间及已存速度元数据不变，成功后清除旧关节逆解、全逆解/分层图及关联CDF初始解。原世界坐标整体变换接口和默认模式保持。
- Release/Debug 主程序和领域回归构建通过，两配置各通过ProcessPoseImport、Architecture Boundary、ConfigurationTabs三项检查。领域覆盖三个不同姿态的XYZ±90/450度、反向恢复、轴/位置/元数据不变及错误原子性。原MainWindow实测7056点矩阵Z+90（Release/Debug），四元数样例X+90、Y−90及旧整体世界变换（Release），五项导入-真实弹窗-变换-导出回读-删除检查全部通过；位置最大回读误差4.62e-16m、姿态矩阵误差2.73e-15、时间误差0。截图核对真实轴下拉/角度/帮助文字和更新后的RGB姿态。
- 初次诊断删除断言错误地要求项目所有轨迹清空，现改为仅断言所选导入轨迹被删除（私有测试项目已有其他轨迹）；第一次重跑被文件覆盖确认阻塞，仅关闭本轮带profile参数的测试进程，后续使用独立输出名通过。没有修改生产删除行为或关闭用户程序。
- 原SDK DLL部署覆盖已恢复本轮初始备份，Release/Debug全部DLL哈希与初始相同；外层Release已备份两项不一致依赖并同步已验证的Release运行库。C++七文件CRLF及主仓/两个子仓diff --check通过。原有未提交姿态显示与其他业务改动保留。
- 新版入口：C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewerrx64.exe、RobotQtViewer_LocalRotationrx64.exe；SHA256=AA1E0C97D78C470FE8D476FFB112C81F7B0DBAE0173FD1EB8ACF8AF07B2C2143。外层旧主EXE已备份，本轮交付清单build/trajectory-local-rotation-delivery.json；证据为build/local-rotation-{z-release-verified,x-release,y-release,world-release,z-debug}.json及截图/日志。未改用户桌面轨迹输入。

## 2026-10-13 STL 导入单位选择（已完成）

- 用户要求选完STL文件后选择米/毫米。UI对话框属于ProjectAssemblyDialogService，MainWindow只编排选择；SceneEntityWorkflowController在同一项目mutation内设置SceneObjectDesc.visualScale/collisionScale为1或0.001，SDK和项目格式不变。取消不修改项目；非STL保持原行为，旧接口调用默认米。
- 修改前备份于外层build/backups/stl-import-units-20261013，保留现有姿态/轨迹变换等未提交修改。验证米/毫米、保存回读、取消和失败回滚、原MainWindow实际导入；Release/Debug编译及相关回归，恢复部署覆盖的原SDK DLL。

- STL单位功能已完成：Import Object选完.stl/.STL文件后弹出“选择STL模型单位”，可选米(m)或毫米(mm)，默认米。毫米统一按0.001换算，取消不新增对象。非STL仍直接导入；旧workflow调用保留默认米，应用静态库增加typed SceneObjectImportUnit参数，未修改预编译SDK、依赖或项目格式。
- 单位转换由SceneEntityWorkflowController同一原子项目mutation写入已有SceneObjectDesc.visualScale/collisionScale，显示与碰撞采用同一比例；之后沿用viewport reload、失败snapshot restore、dirty和视图通知。没有改写STL顶点源文件。
- Release/Debug主程序及RobotQtViewer-RibbonPcdSmoke构建通过，两配置各通过Architecture Boundary和导入回归（米/毫米双scale、项目保存读取、恢复导入快照、非法单位不修改项目、默认调用）。原MainWindow两配置均完整验证真实Import Object文件选择→取消/米/毫米对话框→视口重载→Save Project As并用SDK回读：两个单位scale正确、取消不增加对象、只新增2对象全部通过；截图核对两项选择和解释文字。使用私有配置副本保留完整ABB/A.STL场景，只移除无关轨迹表，未使用简化窗口或改用户项目。
- 证据：外层build/stl-units-{release,debug}.json、.unit0.png/.unit1.png、.png、.{before,cancelled,after}.sys.json、构建/运行日志。九个源码文件CRLF及git diff --check通过；构建部署覆盖的SDK DLL已恢复，Release/Debug全部DLL与本轮初始备份哈希一致。外层Release两项不一致依赖（lib_coacd.dll/tbb12.dll）先备份再同步已验证C:/b运行库；旧程序备份于build/backups/stl-import-units-20261013，未终止用户运行程序。
- 更新C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe及外层build/Release/bin/RobotQtViewerrx64.exe、RobotQtViewer_StlUnitsrx64.exe；SHA256=D1D96AF925A9A4D9F8CEF6DD9A299DA9B6F5D917FC2A6A2C51499A7BBA66587E。保留此前姿态轴/局部姿态变换等所有未提交改动；交付清单build/stl-import-units-delivery.json。

## 2026-10-14 内表面喷涂引导共用 CDF 按钮（已完成）

- 将 ryh 原全逆解/Top-1 后的内表面姿态/喷距候选、实际FK数值IK、安全节点DP、APF/一轮QP、局部定时和完整验收移入 ProjectMotionPlanning 的纯C++共享服务。GUI 与 Freeform_APF_QP 的 guided 入口调用同一 repair 接口；不返回旧最终文件冒充计算。旧无profile调用保持原流程。新增可选profile/报告字段和profile发现接口，无新第三方依赖、预编译SDK或DLL接口修改。
- CDF新增“内表面喷涂姿态引导（允许调姿与喷距）”开关，当前固定ABB4600+A.STL场景自动发现校准文件并默认启用。启用时明确使用profile的0.1mm软余量、一轮QP，恢复普通模式后仍10mm默认。所有者集中核对资产哈希、基座/工件位置、实际喷枪TCP及完整无过滤Exact检测器；过滤Link6或工件移动10mm均拒绝且不发布最终解。
- 原MainWindow完整真实按钮流程：原ryh 2600点→全逆解→全局Top-1（cost=646.36190669648）→共享优化7362点。原生共享服务结果与GUI导出最大差5e-7度、5e-7秒，为显示导出舍入。最后过滤保护与统一阶段指标改动未重复长时间几何优化；最新Release原生入口已独立重新验收全部最终轨迹。计算来源和验收身份分开保存在 completed.json，未伪造当前binary重算身份。
- 0.0001rad密验收2088538状态：碰撞、内表面喷射方向、关节限位、离散速度/加速度违规均0。最大原有序TCP偏移91.255939mm，起终位置误差9.85e-9m/姿态2.13e-9rad，节点最小间距0.108300mm；运动位移界证书整段下界0.010654mm。最终重定时1188.15214s。范围为ABB4600含喷枪对A.STL的关节直线插值，FANUC L/CNT控制器插补未验证，不能承诺10mm或工艺厚度一致。
- Release/Debug各5项回归通过（SprayGuidance、Apf、TrajectoryQuality、LayeredIkGraph、Architecture）。最新Release与已发布外层EXE在原MainWindow开关检查通过，最终7362/7362点原窗口完整播放、碰撞/无效0并保存录像。完整按钮墙钟3531s，曾并发其他规划，不能作性能承诺。17个涉及C++/CMake文件ASCII/CRLF核对和三仓diff --check通过；原有姿态变换等未提交修改保留。
- 最终Release/Debug主程序、领域库、外部原生入口已构建。首次Release多目标命令尾部写错测试target名称，随后用正确目标成功构建，不属于源码编译失败。构建部署覆盖的coacd/tbb已按初始哈希恢复；一次复制时本轮验收进程尚未完全退出，待其退出后恢复成功并重新执行全密验收。最终每配置42个DLL与初始一致。临时CMAKE_PROJECT_INCLUDE已清除，未修改SDK。
- 已同步 C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe 与外层 build/Release/bin/RobotQtViewerrx64.exe，并提供 RobotQtViewer_SprayGuidedrx64.exe，SHA256=D3DAECAF073938C9F1EC663476882E033D65527BE48A89B61812A2450345CF71。外层旧程序已备份；本轮交付清单 build/spray-guided-button-delivery.json。
- 外部 run_ryh.py --force 从原Cartesian与原Top-1重算共享服务，--force --rebuild-guidance重算全逆解/Top30。默认绑定输入、Top-1、引擎、规划库、模型和输出哈希复用。output/final已更新为共享结果，旧结果完整备份 previous_before_shared_button_*；缓存复用、TXT/LS序列化、原窗口动画及最小距离/分阶段图通过。浮点优化与旧手工脚本结果不保证逐字节一致（最大关节差0.932641度），没有宣称同一文件。
- 证据：C:/b/rs105-merge/spray-guided-{main-window,release-controls,published-controls}.json、final-independent-validation.log、final-{release,debug}-tests.log；外部 runs/ryh_20261014/output/final/{validation.json,adaptive_motion_validation.json,workflow_comparison.json,export_validation.json,main_window_playback.json,robot_animation.mp4}。使用说明 docs/cdf_inner_surface_guidance.md。

## 2026-10-14 Trajectory Transform 导出 ABB/FANUC 程序（已完成）

- 用户要求外部脚本打开的Trajectory Transform分页支持new.txt位姿直接导出MOD/LS，沿用CDF模板。ProjectMotionPlanning新增ProjectRapidTrajectoryExporter::generateCartesian(plan)和ProjectFanucTrajectoryExporter::generateCartesian(plan,options)，不需要机器人FK、关节结果或逆解。旧joint接口保留全部模型/工具校验；ABB提取共用序列化，FANUC以共用生成器处理两种输入，未复制另一套格式逻辑。MOD为tool0/wobj0/MoveL/v50/z10，LS为UF2/UT1/L/400mm/sec/CNT100；原时间戳不作为控制器定时。
- UI/controller新增两个导出意图/按钮，读取不可变选中Cartesian计划后弹保存框，按格式补扩展名、FANUC校验名称并用QSaveFile原子保存。允许原轨迹或整体/局部轴变换后的轨迹，空/关节-only/播放中禁用；导出不改项目dirty、源数据、机器人关节或旧逆解。相关帮助/结果标签使用PlainText和可压缩宽度，长文件名不增加分页最小宽度。
- Release/Debug主程序、FreeformPlanner、两导出回归及ProcessPoseImport构建通过。每配置RapidExport/FanucExport/ProcessPoseImport/Architecture四项通过，旧与新接口完整程序内容比较通过（相同目标），覆盖1007点WPR正负奇异极点、turn/符号、空/非有限/非刚体位姿、非法晚行原子失败；实际模型CDF导出保留验证。
- 使用原MainWindow真实导入/变换/保存按钮验证：Release new.txt 7362点原始、局部Z+90、世界平移旋转；Debug new.txt全部点原始和4个位置/四元数目标局部X−90。5组MOD/LS全部位姿回读通过，保存前后Cartesian矩阵文件一致。ABB最大位置误差8.42e-10m/姿态1.88e-9rad，FANUC8.43e-7m/1.88e-5rad，符合模板舍入；输出ASCII/CRLF、点序与指令数匹配。首次Debug大轨迹驱动没有完成报告，随后使用独立目录和捕获进程退出码重跑全部7362点通过；未将未完成试验算作通过，也未修改碰撞/渲染算法。
- 用户旧Release窗口运行期间，在Release/Debug transform-export-bin隔离部署，未终止/覆盖其文件；新版链接后一次post-build复制因本轮Native复验占用coacd/tbb而失败，待复验完成后成功重跑部署并按初始哈希恢复隔离运行库。原/新目录每配置42个DLL与初始一致，SDK未修改，无新依赖；本轮11源码ASCII/CRLF及三仓diff --check通过，其他未提交修改保留。CMAKE_PROJECT_INCLUDE临时构建钩子已清除。
- 外部run_ryh.py使用新隔离BIN/ENGINE，record_main支持指定原窗口入口；默认脚本结果缓存因规划库与入口变更，用最新Native重新独立完成2088538状态无碰撞/方向/限位/离散动态违规验收。几何未重优化、原final未替换，completed.json的verification_history记录新验收身份/日志，保留原计算来源；缓存回放成功，不强制用户重跑长时间QP。
- 用户原窗口自行退出后同步C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe、外层build/Release/bin/RobotQtViewerrx64.exe及两个TransformExport别名；脚本使用C:/b/rs105-merge/Release/transform-export-bin/RobotQtViewerrx64.exe。全部Release入口SHA256=F25787AC9F4406CCCFEDBF1308F86F89E3FDEE531C1712612858DF3BCB99E211。旧程序/源码备份build/backups/transform-program-export-20261014，交付build/transform-program-export-delivery.json。
- 证据C:/b/rs105-merge/transform-export-{new-release,new-local-release,new-world-release,new-debug-final,quaternion-debug}/{report.json,roundtrip_validation.json,MainModule.mod,XPORT.LS}和截图，release/debug构建/回归日志及cache-validation/cache-replay日志；外部README已更新使用方式。未验证控制器编译/IK或插补碰撞，此功能输出给定位姿，沿用模板构型占位与工具/工件标识。


## 2026-10-15 ryh 位置贴合、连续关节与安全减点（已完成）

原桌面 `ryh.txt` 的2600点和固定ABB4600、喷枪、A.STL、基座/工件位置及完整碰撞过滤保持不变。全逆解与原全局Top-1不变（cost=646.36190669648）；本轮改动属于其后的共享喷涂引导、局部避障/平滑、导出减点与验收。

| 指标 | 旧结果 | 新结果 |
|---|---:|---:|
| 最终输出点数 | 7362 | 3165 |
| 原站点位置RMS偏差 | 70.3mm | 22.0mm（实际关节插值FK复验） |
| 全程最大有序位置偏差 | 91.256mm | 80.132mm |
| 相邻导出位置最小距离 | 0.341mm | 2.046mm |
| 相同40000个源进度的关节二阶差分RMS | 0.020086度 | 0.019393度 |

点数减少57.0%；同进度整体平滑指标改善约3.45%，实际关节路径长度减少约9.55%。不能用不同点数下的最大单步关节角直接判断平滑改善，也不声称每个关节均改善。仍存在约80.1mm的局部位置偏离，不能称全程毫米级贴合。`quality_comparison.json`中21.818mm是TCP折线近似RMS；最终采用`validation.json`中实际关节插值FK的21.9815mm。

最终0.0001rad密检查1891158状态，碰撞/内表面方向/限位/速度/离散加速度违规均0。全部2600原站点均参与实际FK误差评估；起终完整TCP位姿误差约1.01e-11m/1.53e-11rad。节点最小机器人—工件距离0.100412mm，URDF运动界保守连续分离下界0.01000017mm；这些不是10mm安全余量。最大工件法线入射角60.998777度，原61度门限未放宽。重定时总长730.52s，仅适用于项目关节轨迹，不是FANUC L/400/CNT100控制器执行时间承诺。

FANUC导出3165点，实际LS小数舍入后最小相邻位置距离2.045866mm，小于2mm点对为0。WPR等价表示连续展开，最大分量变化52.477度；模板仍UF2/UT1、400mm/s、CNT100及原固定CONFIG。未取得ROBOGUIDE具体错误编号/最小距离门限，2mm为本次工程目标；控制器编译、构型解析及L/CNT插补碰撞仍需ROBOGUIDE另验。

实算过程中5/10mm局部APF走廊失败，20mm成功并执行一轮QP；初版最终密方向检查拒绝了略超61度的区间。补充完整区间方向检查、短段合并与正弦平方补修后，最终候选v5全部通过。仅对5个窗口施加小幅J1扰动，最大0.008rad。失败或中止候选均未作为最终结果交付。

本轮按阶段保留了新算的引导与APF/QP几何检查点，随后用最新共享服务重新核验模型/原Top-1/对应关系，再做减点、完整方向补修、运动界证书和独立密验收；没有取旧成功final作为新几何。`computation_provenance.json`透明记录各阶段引擎/输入/检查点哈希，不声称最新单一EXE从IK至QP一次全算，也不声称本轮原GUI完整按钮重算。正常GUI按钮与脚本调用相同改进服务，浮点迭代不保证逐字节相同。

Release/Debug主程序及独立入口构建成功，各8项相关回归通过；最新减点/导出改动后各2项针对性复验通过。Release原MainWindow最终播放3165/3165点、碰撞/无效0、最近距离覆盖层正常并重新录制。SDK每配置42个DLL保持初始哈希，临时CMake注入已清除，无新依赖。三仓原有未提交改动保留。

`Freeform_APF_QP/run_ryh.cmd`默认核验缓存后打开最新版原主窗口；`--force`实际重算，仍可能耗时较长。默认入口为`C:/b/rs105-merge/Release/ryh-quality-bin/RobotQtViewerrx64.exe`，没有替换其他运行入口。最新交付在`runs/ryh_20261014/output/final`，旧final完整归档为同目录`previous_final_*`。分开的原轨迹/旧结果/新结果图、关节对比、最小距离图和原窗口录像均已生成。完整证据及哈希见外层`build/ryh-quality-delivery.json`。

验收范围是当前ABB4600含喷枪对A.STL的关节直线插值、固定三角网格及配置的数值保护量；不是任意环境的无碰撞或全局最优保证，也不是喷涂厚度/连续加速度或FANUC控制器插补验收。


## 2026-10-16 ryh 全往返覆盖与大幅调姿（过程记录，最终验收见下）

用户允许大幅改变姿态、优先原TCP位置，以补回原ryh每次往返的外伸段。实际FK诊断确认旧最终路径100个远端折返点最大回缩63.230mm、99个近端最大回缩22.490mm；站点未被直接删除，主要是表面投影/短喷距引导改变了纵向位置。

共享ProjectMotionPlanning增加默认2mm的轴向覆盖约束，从候选生成、APF提议投影、平滑、减点到密验证使用同一有序参考。允许在profile设置axial_tracking_tolerance_m。APF提议按当前参考段上的自身进度作工艺投影，避免每个小步直接跳到下一站；回填后重新投影并验全部连接。

增加绕工件长轴的喷射方位角候选，使长枪保持轴向插入、自由改变喷射方向；原位置偏差权重由0.08提高至8，保留61度内表面入射规则。后处理调姿图对当前选中连接作SE(3)插值、实际IK/分支和碰撞检验，缓存失败连接并重选；断开连接邻域再增加大方位角/小横移/俯仰组合。原全逆解Top-M图不改。不同探查曲线的等价turn按真实限位对齐并复核实际FK，避免无意义整圈。

新增可选tcpStateProjection源码接口（默认空）和私有guideObserver诊断回调；无预编译SDK变更、无新依赖。安全候选复用nearestDistance的同状态碰撞/距离结果，最终认证检查不减少。Native独立检查全部原始站点/密样本覆盖并输出coverage_validation.json，脚本将其纳入缓存验收；guide_checkpoint只有引导节点，不能作为最终无碰撞路径。

azimuth_v1至v4诊断未交付：初期窄覆盖提议及调姿连接不连续。v4虽有约2.065mm引导RMS位置误差，仍有4处插值连接不通及约119度单步，已停止，旧final保留。最新azimuth_v5从原ryh和原全局Top1完整计算：局部扩充后第二轮惰性连接检验未解决数为0，引导最大单步33.877度，2594/2600站点位置误差小于1mm、RMS1.676mm、最大56.768mm、最大轴向误差0.000896mm；APF在5mm局部走廊完成全路径检查，当前平滑/QP及完整认证尚未结束。上述是引导/中间结果，不能冒充最终验收。

Release/Debug主程序、Native及四项相关回归构建通过；APF、SprayGuidance、TrajectoryQuality、LayeredIkGraph各配置均通过，包括窄轴向覆盖下薄障碍分步逃逸、投影限位不可行拒绝、横向自由度和完整源顺序。SDK每配置42个DLL与初始哈希一致（部署覆盖的Debug coacd/tbb已恢复）。CMAKE_PROJECT_INCLUDE临时注入已清除，源码CRLF/ASCII及两仓diff检查通过。最终发布待完整碰撞/方向/动态/全部原站点覆盖验收及原MainWindow复核，尚未替换旧output/final。


## 2026-10-16 完整覆盖结果（已完成）

使用未改动的原桌面ryh.txt、固定ABB4600/长喷枪/A.STL、原全局Top-1，本轮最新Native从调姿引导、APF到一轮QP及最终验收完整重算成功，没有复用旧final或旧APF/QP几何。原主窗口完整播放3432/3432点，碰撞/无效状态0、距离覆盖层302/302帧有效；默认脚本缓存核验通过。原GUI优化按钮链接同一共享服务，但本轮未再次执行其长时间完整重算。

| 指标 | 之前3165点结果 | 本轮3432点结果 |
|---|---:|---:|
| 全部2600原站点实际FK位置RMS偏差 | 21.981497mm | 4.950296mm |
| 原始往返端部最大轴向回缩 | 63.230158mm | 1.429140mm |
| 全部原站点最大有序轴向误差 | 63.230158mm | 1.908945mm |
| 全程最大TCP位置偏移 | 80.132mm | 55.512121mm |

实际调姿引导2594/2600点位置误差小于1mm，但不能把该引导指标当成最终轨迹误差。最终仍有少数较大的横向避让，不能声称每个点位置保持不变。原2600站点全部参与实际关节插值FK检查，轴向覆盖缺省2mm；100个远端/99个近端折返点以及整条起终源范围均核对，不再用站点数或整轨迹端点保持代替覆盖验证。

最终从8172计算节点安全减到3432执行节点，独立0.0001rad检查2268062个状态，碰撞/内表面喷射方向/限位/离散速度与加速度违规0。运动界保守连续分离下界0.010043mm，节点最小距离0.113540mm；不是10mm安全距离。61度内表面入射门限保持，实测最大60.993906度。完整起终位姿误差约9.29e-12m/1.52e-11rad，重定时总长824.705s。

为覆盖端部，大幅调姿增加了部分关节运动。相同40000源进度网格的关节二阶差分RMS由0.019393度增至0.022032度，不能声称本轮比旧结果整体更平滑。相对本轮APF种子，碰撞约束平滑降低了关节长度和弯曲代价；QP仅运行一轮，任何未通过碰撞/工艺检查的步长均不采用。模板LS最小相邻点距1.966112mm，有1对低于工程合并目标2mm；2mm不是用户提供的控制器硬门限，安全/覆盖/姿态约束优先，不能声称所有点间距都超过2mm。

新结果已发布到Freeform_APF_QP/runs/ryh_20261014/output/final，旧3165点final完整归档。run_ryh.cmd默认核验并打开新结果，--force按同一共享方法重算，仍耗时较长。入口为C:/b/rs105-merge/Release/ryh-coverage-projection-bin/RobotQtViewerrx64.exe；没有替换用户其他运行窗口。最终关节TXT、FANUC LS、原11111格式TCP矩阵、最小距离图、原/旧/新独立覆盖图、端部对比和原窗口动画均已生成。

Release/Debug主程序和Native构建通过，各4项APF/SprayGuidance/TrajectoryQuality/LayeredIkGraph回归通过；SDK每配置42个DLL保持初始哈希、临时CMake注入清除，无新依赖。当前项目静态源码接口增加可选tcpStateProjection，普通无回调调用保持原逻辑；未改预编译SDK。三仓其他未提交修改保留。

验收范围仍为当前ABB4600含长喷枪对固定A.STL的关节直线插值、刚体网格和数值保护量。FANUC LS仅作格式/位姿回读，ROBOGUIDE控制器的构型、工具/工件标定及L/CNT插补碰撞未验证，不可把此证书用于另一机器人工作站。详见外层build/ryh-coverage-delivery.json及最终目录内验证文件。

## 2026-10-16 ryh1 新轨迹兼容与原主窗口完整流程（进行中）

用户要求使用桌面ryh1.txt，实际通过run_ryh场景、Basic Planning原始导入、全逆解、全局Top-1和CDF按钮得到无碰撞路径。保留此前所有未提交代码、旧ryh及已验证final；本轮优先改外部输入/任务编排，不先改共享规划算法。

新文件为2400个刚体矩阵，位置mm、时间严格递增，范围X=1100.171至2145.508mm，含比旧轨迹更长的外伸段；原位置/姿态均不改。独立Native全IK2400/2400，每层8解、共19200，实际FK位置/姿态误差均小于1e-6，原全连接分层图全局Top-1 cost=712.190545575791。

外部新增trajectory_job.py，按源内容SHA建立隔离runs/ryh1_95c130c7dd8d快照，复用已标定相同几何/位姿/过滤，scene仅重定位输入路径。run_ryh.py支持--trajectory和--scene-only，覆盖验收不再写死2600点；无Top-1时自动全IK/分层图，只有完整验收成功才切换默认活动任务。旧缓存仅允许已知旧launcher哈希的兼容迁移，要求引擎/模型/输入/规划库/Top-1及全部输出哈希相同，不冒充重新规划。

原MainWindow PID42844已从原ryh1.txt执行真实导入、全IK和全局Top-1选择（代价一致），目前正在原CDF按钮的姿态引导/后续APF一轮QP。用户原窗口PID18144保持运行。新结果尚未验收，不能称已无碰撞；未重算旧ryh或修改SDK。后续对实际GUI导出结果独立密碰撞、原2400站点覆盖/动态复验和原主窗口播放，通过后再发布与记录。


### ryh1 首轮真实 GUI 失败及有界 APF 扩展（进行中）

原 MainWindow 全流程失败，2811.2s，QP未启动；2400原站点全部恢复于11084点加密参考。Top-1原节点951处碰撞；调姿引导最大位移19.696mm，但143处SE(3)连接尚未连通。APF在加密47至50区间、5/10/20/30mm均失败，失败导出仅作诊断。

共享SprayGuidedRepair原局部走廊写死上限30mm，未使用原100mm总预算的剩余空间。保持窄范围优先，追加50/75mm及剩余预算尝试，局部走廊+最大引导偏移不超过原100mm；不放宽碰撞、61度方向、2mm轴向覆盖或最终验收。仅改共享源，无API/依赖/SDK变化。隔离ryh1-bounded-apf-bin构建Release/Debug，保留PID18144和旧已验证结果。尚未证明新轨迹可行。

进一步定位：原加密参考前170个节点无几何碰撞，但48/49/55/157喷射方向不通过。独立密过程检查首失败source_time=2.353410869s，方向余弦0.939592685小于cos(20度)，不是无法求IK或必须改变末端位置。工件端沿使原横向射线错过有限内表面，APF只有几何距离力及工艺bool拒绝，不能可靠自行生成前后倾姿态。

在后Top-1姿态引导的首次断开连接扩充中，先增加保持位置的±20/30/45/60度轴向倾角与7种方位角组合；第二轮仍断开才扩充原位置/小倾角候选。原全逆解和Top-M分层图不变。新增有限端沿回归确认同TCP位置下倾斜恢复内表面命中、20/61度门限不变。Release/Debug主程序和Native编译通过，各4项相关回归通过，SDK42DLL/配置已恢复初始SHA。当前PID36952原MainWindow v3正在重新执行ryh1全IK/Top-1/CDF，尚未发布成功结果。v2诊断与我方窗口已主动停止，记录于bounded_v2_aborted.json，用户PID18144保留。

本轮用户再次明确允许大幅调整姿态、位置尽量不变。继续保持固定ABB4600/长喷枪/A.STL及全部2400原站点；不把扩大调姿误写为放宽碰撞或删除外伸段。v3真实MainWindow已完成全部IK/Top-1，目前在连续调姿搜索，首批零偏移大倾角仍有断开，正在扩充局部横向候选。未发布final，旧活动任务和用户窗口保持。外部launcher已指向隔离新运行目录，活动任务只在完整验收成功后切换。


### ryh1 主动固定位置调姿修正（进行中）

v3原MainWindow完整运行至1953s仍有146个调姿连接未通，首个5mm局部APF区间失败，已主动终止我方PID36952，未证明更宽走廊失败；二进制/静态库/日志保存在外部before_aim_projection，用户PID18144保留。

根因是APF现有喷射方向判据只有拒绝，没有朝向内表面的主动投影。私有SprayInnerSurface新增保持TCP位置的射线目标候选，按实际内表面最近三角形、最短转动生成姿态，并继续使用原有正侧/20度喷射锥/61度入射判据。共享局部实际模型IK将候选投回合法关节；APF更新投影与后Top-1连接探查共用，之后完整碰撞/路径检查不变。无SDK或公开接口变化。新增有限端沿、姿态合法、TCP不动及外侧拒绝回归；Release/Debug主程序/Native构建及各4项回归已通过。外部Native正在用真实v1失败引导诊断新修正，尚未发布ryh1结果；仍需原主窗口完整重算和独立验收。

进一步诊断显示，只对APF小步做调姿仍会在原关节弦的工艺无效站点停滞，因此共享喷涂服务在APF前增加实际模型笛卡尔连续IK预处理（20mm平移、0.04rad姿态/关节跨度），每个原始站点原时间和原引导关节均保留；只有整条局部探查的节点均通过实际FK、限位、方向和碰撞才采用，失败边仍交APF。该预处理不把安全节点当作整段安全，后续所有连接继续APF/运动界/密检查。已有细分后仅关闭额外固定插入一个样本，仍保留0.04rad自适应细分及0.00025rad各阶段碰撞检查。射线修正沿最短转动二分寻找最小可行转角，并留1度内部角余量，避免轻微越界直接跳20度。Release/Debug主程序、Native和各4项回归通过。v4原MainWindow PID34348正在从原ryh1全IK/Top-1重算；Native PID21016复用旧失败guide仅作定位，2267边完成笛卡尔节点预处理、132边仍交APF，不是最终无碰撞证书。


### ryh1 APF 逃逸场提前退出修复（验收进行中）

2026-10-16：共享ApfLocalPlanner在某个确定性扰动投影失败时直接return false，导致其余关节方向全部未尝试。现改为仅跳过当前场，保留其他逃逸方向。新增固定锚点、圆形障碍物与轴向投影受限的回归，要求轴向扰动失败后仍由横向场连通，Release/Debug APF及Debug四项领域回归通过。完整ryh1无碰撞结果尚未证明；Native诊断明确复用原失败引导而非旧final，用户PID18144及旧已验证输出保留。


### ryh1 中间瓶颈自适应调姿分层图（进行中）

固定TCP首个困难位置(2.018048870,0.599301194,0.564960684)m、源时间2.353410869s，按实际模型探查1105个方位角/俯仰/滚转姿态，686个IK有效，22个同时无碰撞且满足原喷涂判据。证据保存在外部rim_pose_grid，不是完整可执行轨迹。说明原站点之间存在离散姿态瓶颈，单靠原站点姿态图和事后APF可能选错通道。

共享服务仅在Top-1后的喷涂调姿图内，自适应插入首个不合格笛卡尔探查站点，补充同样实际FK/限位/完整碰撞/工艺筛选的姿态候选，再运行原二阶引导选择。原全逆解/Top-M图及全部2400导入站点、顺序、位置参考不改变。补充临时站点来自原笛卡尔轨迹的有序插值，不从关节FK冒充输入。最多四轮加点，之后仍APF、一轮QP和独立最终验收。

同时修复固定位置调姿投影的非单调可行性：完全对准三角形可能超出61度，但部分转角已经满足20度喷射锥；现先沿旋转弧找可行区间，再二分最小转角，原20/61度门限不变。新增相应有限端沿回归，Release/Debug四项领域回归通过。内点姿态连续携带和固定位置局部搜索仍接受全部最终检查。旧final及用户PID18144保留，v4实际GUI失败、v5/v6中止和各Native诊断均有记录；未发布ryh1无碰撞结论。


### ryh1 APF位置投影顺序与原始站点覆盖（验收进行中）

APF扰动原先先做工艺/喷射投影，再恢复TCP走廊；偏离工件内腔的有限提议会在可恢复位置之前被提前拒绝。现改为先恢复普通位置走廊，再做工艺投影，投影后再核对位置；已在走廊内仍必须执行更窄工艺投影。新增定向回归覆盖上述两条路径，原逃逸场失败后继续尝试其他方向的回归保持。Release/Debug四项领域回归通过。

新增可选profile axial_tracking_scope=source_stations，缺省all_states保持旧行为。仅试验配置允许点间连接在原100mm走廊内绕行，全部原始2400站点及往返端部仍逐点验证2mm轴向覆盖；减点新弦也显式查每个原站点。锚点匹配用按X排序的范围查找，避免每个密样本遍历4800锚点。尚未证明该配置可生成完整ryh1轨迹，不能当作完成结果。

原MainWindow v7与Native为重复旧算法；停止我方v7 PID16680并存gui_workflow_v7_aborted.json，Native21992继续计算，原用户PID18144保留。隔离scope运行目录测试失败引导恢复，不替换旧ryh已验证final，不把checkpoint诊断称为原输入完整重算。


### ryh1 喷射余量势场与局部实测（完整验收进行中）

共享APF增加可选signed process clearance，用于前瞻斥力与有限差分梯度；喷涂服务按实际内表面有向距离、20度锥方向余量和61度入射余量计算，不以势场值取代几何碰撞/整段工艺硬检查。普通调用无回调时保持原计算路径。新增“几何无障碍但工艺禁区需要绕行”回归，Release四项回归通过；Debug主程序/Native已构建，最新版复验仍在进行。新增源码options字段tcpStateClearance，未改预编译SDK或依赖。

固定位置优先的强空空间斥力另测在attitude61，未纳入当前算法（实验被撤回）；实际处理保留原边界抑制策略。外部profile新增可选apf_local_corridors_m、guidance_adaptive_passes、guidance_connection_passes，缺省仍为旧5/10/20/30/50/75mm、4轮插站和12轮连接搜索。当前试验process_corridor61从20mm开始，仅保留全部原站点覆盖，不超过原100mm总预算；不改变原全IK/Top-M定义。

第一局部24原站点诊断在20mm走廊连通54至56困难段，safe anchors42至68，并通过APF整段检查和一轮QP；该局部截取终点不等于原全局终点，最终位姿检查如期拒绝，不能作为完整最终轨迹。80度入射试验也只用于诊断，没有纳入当前61度配置。

当前Native用全2400原输入+原失败GUI-v4引导检查点重算完整APF/QP；原MainWindow v10 PID26256从原ryh1导入、全IK、原全局Top1、生产CDF按钮重新计算，不使用引导检查点。两者结果分别记录，不声称相同几何。原用户PID18144及旧ryh输出保持。尚无ryh1最终证书或可执行final。


### 2026-10-17 ryh1 区间独立走廊与中断后恢复（验收进行中）

CdfQpTrajectoryRepair原先某区间失败后由喷涂包装层从头放宽整条轨迹，丢失已验证局部连接并重复密查询。现对带喷射余量回调的每个区间按20/30/50/75mm及剩余全局预算独立尝试，各自再做三档安全锚点扩展；只保存经过全段几何/工艺检查的替换，边界关节与时间/点数保持。普通APF没有新回调时仍只用原配置走廊。全局100mm预算、全部原站点2mm轴向覆盖和最终原位置/FK检查保持。当前profile可只运行一次最大总预算的服务，局部先窄后宽，不因一段困难重复整条路径。

前轮被外部中断，恢复时PID41504、18076、26256及原用户PID18144均已不在，不能据此推断任何轨迹成功；部分日志与检查点保留。原ryh1哈希未变。Release主程序/Native已编译，四项领域回归通过，隔离ryh1-local-bands-bin启动Native28008（完整原输入+先前失败引导，重新APF/QP）及原MainWindow v11 PID16872（原始导入、全IK、全局Top1、生产CDF按钮，无引导检查点）。Debug构建/复验正在执行。尚无ryh1最终验收，旧ryh结果与默认活动任务保持。

### 2026-10-17 重复边沿搜索宽度提示（完整验收进行中）

完整local-bands实测1039及1187附近区间在20mm的三档锚点/14个场耗尽后由30mm通过；1304附近进一步由50mm通过。共享CdfQpTrajectoryRepair仅增加本次repair内的空间邻域走廊顺序提示：120mm内最近成功区间的宽度优先，其余宽度完整保留，且重新计算全部关节路径/几何/工艺检查。不缓存或跳过新段碰撞验收，不改变100mm总预算、原始2400站点覆盖、20度喷锥与61度入射门限。普通无喷涂余量回调路径不变。

Release/Debug主程序与Native构建通过，各四项APF/SprayGuidance/TrajectoryQuality/LayeredIkGraph回归通过。首次build调用误写测试target名，已按现有带连字符target修正并构建成功；不是编译缺陷。新输出隔离为ryh1-regional-bands-bin，旧运行二进制/静态库及源码备份保留。80度Native对照在无完整结果时主动停止并记录local_bands80_stopped.json，不把中止当成规划不可行；主要61度Native、原MainWindow v11和新版v12继续实际流程测试。模型/配对/输入等价检查通过，80度试验仅入射门限不同，未发布到默认配置。当前仍无ryh1完整final。


### ryh1 喷射势场归一化试验撤回（完整验收进行中）

局部24点诊断曾将喷射方向余量乘40以增强势场；原方法在20mm局部走廊连接成功，而归一化试验耗尽20mm搜索后需30mm，未改善瓶颈，已撤回三个相关源码/测试改动。局部诊断之后完成APF/单轮QP，但因截断终点与原全局终点不同而被最终端点检查拒绝；不将其作为完整失败证明或可执行final。试验进程已自然退出，不重复终止已消失PID39760。SprayGuidedRepair.cpp已与试验前备份逐字节一致。当前regional-bands Native20376与原MainWindow33672仍采用未含归一化的隔离二进制继续运行，不重启长任务。

编译目录的共享静态库曾被试验构建覆盖，正在隔离重建撤回后的源码；运行中regional二进制的计算版本按regional_bands_identity.json与before_process_scale冻结库哈希记录，不能以当前全局库哈希冒充。原输入、模型、碰撞过滤及旧ryh已验收结果不变，ryh1仍未取得完整final。


### ryh1 连续步长对照与外部导出核对（完整验收进行中）

另对相同前24源站点进行0.01rad笛卡尔连续步长诊断：内部规划节点从109增至289，但未连通原边仍为2，初始不合格区间仍为2，未消除瓶颈，已停止我方隔离PID16528并撤回可选step参数，SprayGuidedRepair.cpp恢复为采用版本。不是新的完整轨迹或不可行性证明，两条既有长任务未中断。

外部run_ryh.py新增按任务读取冻结runtime.json的可选选择，核对实际EXE/DLL哈希，规划静态库身份取冻结计算记录，避免后续试验build覆盖全局lib后错误标记旧程序。ryh1尚无完整final，runtime_candidate.json仅准备未激活，默认活动任务未改。finish_gui_run.py改用同一冻结库身份。verify_exports.py增加实际FK生成的11111矩阵格式TCP导出和回读验证；原站点位置误差改用Native实际关节插值FK验收值，TCP直线/Slerp估计单独标注，避免把估算值误称实际原站点误差。对旧ryh结果的独立scratch拷贝进行格式回归通过，没有改变其final，也不作为ryh1规划成功证据。

撤回势场归一化后的隔离regional-rebuild Release/Debug主程序/Native构建通过，各四项领域回归通过。SDK DLL缺失部署检查已增加缺文件恢复，42DLL/配置SHA核对通过。当前仍保留regional-bands Native20376、原MainWindow33672，任务尚在运行，未交付无碰撞结论。


### 2026-10-17 ryh1 必要连接段150mm（验收进行中）

用户明确允许必要连接段放宽到150mm，全部2400原始站点保留并报告偏差。共享喷涂规划服务新增可选connection_tcp_deviation_m与source_station_tcp_deviation_m，缺省沿用原调用预算；本次隔离配置连接150mm、原站点100mm、原站点轴向覆盖2mm。保留位移后guide锚点到原点的映射，在APF admissible/projection、减点与最终验收中保持原站点边界；不把移动后的guide点当原始参考。各坏区间仍先窄后宽，再尝试100/125mm及剩余预算，不重启整轨迹。独立Native验证读取相同profile配置并分别报告连接与原站点误差。未证明改善的弧长相位已撤回源码，对照二进制与日志保留。固定机器人/工件/碰撞配对/喷向门限，不发布未验收final。下一步先实测348附近瓶颈，再完整真实MainWindow重算、单轮QP、连续及高密度独立验证，全部通过才激活脚本。

入口轴线候选诊断：432个局部姿态探查有378个IK有效、282个无碰撞，但并非整段可执行路径。新增可选barrel_entry_world几何候选：给定原TCP、入口和喷嘴横向偏置，解析求最近喷向法向使d·n=offset，枪杆g∥d-offset*n且g⊥n，从而保持TCP位置并使长杆经过入口。只扩充后Top-1调姿引导候选，原全逆解/Top-M定义不变；实际模型IK、限位、完整碰撞、喷向、连接APF及最终验收不变。尚未采用为默认或宣称完整结果。

原MainWindow v13暴露外部场景配置传播缺陷：viewer_scene.configure只复制项目文档，未复制其规划profile，发现了workpiece URDF旁的旧100mm配置，故没有使用150mm/入口候选。已在配置生成器携带同一profile并规范化surface/bounds/dependencies/anchors路径；ID别名可逆、几何及配对字节等价断言通过。v13主动中止，不视为150mm算法实验。v14 PID41816真实日志确认原ryh1全局Top1/2400点，实际站点100mm、连接150mm，正在原窗口完整流程。Native3480同步从原Top1重新调姿，无旧guide检查点。

另试入口姿态族补充笛卡尔连续IK失败状态，局部337-360的22条已连接/1条未连、202内部点、93次调姿、5个不合格区间及首批APF日志均未改善，已逐字节撤回该小试验，保留诊断二进制/日志；不作为整体不可行证据。共享源码恢复为正在运行的entry150版本，非未验证的continuation试验。

构建一致性复核：之前copy2恢复ApfLocalPlanner源码保留了旧mtime，MSBuild未重编译Release对象，故connection150/entry150实际使用的是已冻结的弧长相位版本；其实际EXE/lib哈希记录仍正确，但源码恢复说明不完整。没有发布任何ryh1 final。现明确将带喷射余量回调的候选流程统一为TCP弧长扰动相位，静止窗口和普通APF保留原索引相位；不宣称该调整已证明提速。恢复Spray源后显式更新mtime，隔离统一重建Release/Debug，并对实际运行版本的喷涂局部日志做等价复核，避免源代码与脚本算法不一致。原MainWindow v14和Native3480保持其冻结实际版本继续验收。

统一构建Release/Debug主程序、Native及各四项领域回归全部通过。原MainWindow v15使用统一源码、完整原始ryh1导入/全IK/全局Top-1/生产CDF按钮，实际日志确认原站点100mm、必要连接150mm；v14旧顺序任务主动中止并留记录。区域走廊提示将整个列表旋转，优先提示宽度及更宽允许范围，再保留较窄回退，避免100mm失败后先耗尽20/30/50/75mm。未删任何场、锚点扩展或碰撞验收。完整最终结果尚未产生，默认用户入口及旧已验证结果未切换。

CMake正常配置恢复成功，RobotQtViewer运行库部署ON，临时CMAKE_PROJECT_INCLUDE及FreeformPlanner部署覆盖已清除。运行库核对脚本补充统一/撤回试验目录，Release/Debug每目录42个SDK DLL重新核对通过；仅补齐缺失/不匹配的基线运行库，没有复制旧EXE。证据位于外部任务output/runtime_verification_unified.log。构建与回归通过不代表ryh1整条无碰撞验收通过。

### ryh1 较大调姿逃逸与严格APF续算（整条验收进行中）

统一Native从新鲜完整引导重新规划，已验证19个修复区间，在加密2077-2080连接处反复停滞。100/125/剩余130.302844mm局部走廊未直接连通，仍保留较窄回退，不能当成不可行性证明。原MainWindow v15全原始流程得到相同引导概况，但实际加密16726点（Native16724），所以两者的检查点不可互换。为使用新增逃逸场，主动停止我方v15并记录aborted；v16 PID44612用隔离large-escape运行时从原输入全逆解/Top-1/生产CDF重新计算，无检查点。尚无ryh1 final。

APF只在六轴、带喷射余量回调且局部走廊不少于125mm时，在原14个场之后追加1.10/1.65rad调姿扰动，按实际限位投影、固定边界及原完整运动/工艺判据验收。普通APF及原14场顺序保持。新增完全拒绝运动夹具覆盖额外场、限位、固定边界和失败输出为空，不能把更大扰动当作已经证明成功。

私有CdfQpRepairCheckpoint.h新增领域层APF续算入口，公开repair接口/Options及依赖不变。重新计算实际模型笛卡尔连续IK及加密参考，逐关节、时间戳、点数、限位、端点与保存参考核对后，才接收未完成APF阶段；全部运动/工艺/最终密验收重做，不沿用旧碰撞证书。QP在APF完整通过后仍只运行一轮。外部Native新增guided-apf-resume，明确区分两阶段未完成APF和三阶段已完成QP，记录阶段来源；没有将失败阶段直接包装为成功。新增拒绝参考、时间、端点、限位、NaN、点数被改动的回归。

Release/Debug主程序、Native及各四项APF/SprayGuidance/TrajectoryQuality/LayeredIkGraph回归通过。六个本轮C++/头/测试文件CRLF核对通过；隔离运行库仅从42DLL基线及配置/Qt插件部署，无旧EXE覆盖。主版本旧冻结二进制继续计算，新的全局静态库身份另记，不冒充旧运行程序的库。新代码尚待真实困难段和整条认证；不创建runtime.json、不切换活动任务。


### 2026-10-17 ryh1 连续IK预处理源构型保持修复（整条验收进行中）

定位旧Cartesian预处理会将中间修正姿态带到后续原始站点，导致71个已选引导构型变化超过1度、最大约125.245度。其失败参考的关节弦FK最远偏离原参考948.488mm；这是APF前失败输入诊断，不是已交付最终轨迹。证据在外部任务output/cartesian_preconditioning_drift_diagnostic.json。

SprayGuidedRepair仅允许插值内点携带修正姿态，每个源边终点必须回到已选安全构型；数值IK单步跨越0.12rad则拒绝整个预处理边，保留原已选端点交给APF。预处理后断言全部源时间及关节值原样保留；不锁死后续APF的姿态自由度。Release/Debug主程序、Native及各四项领域回归通过，正常CMake部署配置恢复成功。私有续算门禁曾在旧相同参考上实际通过，但参考已改变，当前全APF从新参考重新算，未挪用旧部分路径。

Native46864实际确认全部2400已选构型保留，2244条预处理边、155条交APF、11744内部节点；密扫描12815点，182个不合格区间（旧参考16724点/238区间）。原MainWindow v17 PID30652从原ryh1导入、全IK、全局Top-1、生产CDF按钮独立重算，无引导检查点。以上只证明预处理修复和工作量变化，尚无整条无碰撞final，runtime.json及默认活动任务未切换。


外部report.py补充独立original_path.png、apf_path.png、final_path.png（统一视角/坐标范围）以及全部原站点实际FK位置/轴向误差图original_station_tracking.png。只读取真实验证CSV，不用TCP线性插值估计冒充FK误差；旧ryh文件仅复制到显式old_data临时目录做图像排版核对，旧final未改，也不作为ryh1成功证据。外部报告改动不影响正在计算的领域算法/运行时身份。


### ryh1 私有入口姿态逃逸场与轴向限制对照（实验，未采用为最终流程）

完整endpoint-guard Native46864继续运行，已通过17个局部段并越过旧参考2077对应瓶颈；原MainWindow v17 PID30652仍独立计算。其冻结18ff9ac9库身份、二进制和原2mm轴向限制保持，不把后续全局库冒充该运行版本。默认用户任务和旧已验证结果未改。

为处理后续局部极小值，在六个领域私有源/头/测试文件中添加ApfSeedFields：原14个关节方向场之后、较大通用场之前，尝试绕标定入口的18个保持TCP位置姿态族，并分别从两端构造吸引参考；模型IK、合法turn和限位筛选提议，最终仍由原完整运动/工艺/走廊检查接受。仅扩展私有repairCdfQpWithCheckpoint入参；公开服务Options、SDK与依赖不变。失败族继续其他族，越限/非有限提议不得进入oracle；固定端点和输出点数保持。Release/Debug主程序、Native和四项领域回归全部通过，源CRLF和SDK42DLL基线SHA核对通过。构建正常配置已恢复。当前入口场局部331-420点诊断PID16872仍在运行，不能称已改善整条轨迹。

另用局部365-379源点、原TCP位置和相同安全引导，隔离对照既有可配置axial_tracking_tolerance_m=20mm；原站点100mm与连接150mm球形位置界仍保留，模型/碰撞过滤/20度喷射锥/61度入射不改。该20mm轴向不是已采用参数；优先保留仍在推进的原2mm完整结果。首次对照PID29068因独立Native固定名inner_surface.json未复制而未进入规划，记录setup_failure；复制同SHA几何与运动界后PID5608重试，不能误称算法失败。所有裁剪窗口仅诊断，端点姿态检查可能预期拒绝，绝不能发布为完整final。

外部finisher补充传播显式axial_tracking_tolerance_m（未配置则移除并使用默认2mm），防止将来验证采用配置与手动流程profile不一致；当前v17仍读取原entry150配置。当前源hash在entry_fields_source_hashes.json，隔离runtime_entry_fields_candidate.json；原endpoint源码/库备份before_entry_fields。尚无ryh1可执行最终结果。


### ryh1 原2mm轴向的局部连接对照（全轨迹未验收）

2026-10-17：v14入口几何参考允许碰撞的内部吸引提议进入APF，实际运动接受门禁不变。源365-379局部在75mm走廊完成APF、一轮QP、保守连续几何与高密度喷向检查；裁剪端点不等于原Top-1端点，最终端点检查按预期拒绝，不作为完整final。放宽20mm轴向的局部也连通，但不必据此采用；完整20mm仅隔离对照，原2mm完整Native46864和真实MainWindow30652继续运行。

后续v15私有APF尝试在单个入口参考点生成失败时保留该点原参考而继续后续有效提议，至少一个有效提议才启用，所有接受边仍完整核验。增加缺口回归、入口拒绝原因限量日志；Release/Debug主程序、Native与四项精确匹配领域回归通过，源CRLF保持。隔离局部44868尚运行，无整条证据，不激活脚本默认任务。一次测试目标拼写错误已更正；一次过宽测试正则启动的Periodic测试已主动停止，随后四项精确测试全通过。


### 2026-10-17 ryh1 旧已验证姿态通道参考（v19，整轨迹验收中）

用户确认新轨迹仅扩展两端。实际核对旧2600、新2400点；有序笛卡尔对应动态规划确认大部分原点完全一致，新增开口段距旧原点最多100.0012mm。v14完整APF仍在源395附近耗尽，QP未开始；v18真实主窗口消失且没有完成报告，不能称通过。v16/v17局部都完成APF/QP但裁剪端点不等于整轨迹端点而拒绝，不能发布。

共享SprayGuidedRepair新增可选attitude_reference_file及FNV1a64校验；参考文件明确每个当前原点、旧原点及旧实际runtime弧度关节。只取旧实际FK姿态与偏移作提议，平移到当前原点重新实际模型IK、合法turn/限位、完整碰撞与原喷向检查。首尾仍用当前Top1原位姿；未通过的新端部周围继续旧候选族和连接搜索。公共段保留既有姿态通道，所有连接仍重新APF、单轮QP、连续及密验收，不继承旧安全结论。viewer_scene.py传播该可选路径。无公开API、依赖或SDK变化。

Release/Debug主程序/Native构建及各四项领域回归通过。Native35548首次运输2316/2400节点通过，剩余候选在求；原MainWindow46300从原ryh1导入、全IK、全局Top1、生产CDF独立重算。外部冻结runtime_prior_attitude_candidate.json；默认活动任务和runtime.json仍未激活，旧ryh final不改。CMake目前仍为prior-attitude隔离注入，交付前须恢复正常并核对Debug/Release42SDK DLL。


### ryh1 已验证曲线中间节点的姿态参考（v20，完整验收仍在进行）

v19仅转移离散姿态会移除旧路径中间的安全弯曲，且不必要地将原2mm允许范围内的轴向偏移归零。未发布v19，主动停止Native35548和真实GUI46300，原日志保留。v20参考增加旧实际关节直线曲线中间节点，2400原站点全部保留，附加2072参考节点，共4472。对已满足2mm限制的旧偏移保持，只在越界时恢复轴向；优先保留第一个完整通过的旧参考姿态，失败处才补充新姿态。仍对全部实际运动重新验碰撞、工艺、位置和末端位姿。

共享私有实现支持有序source_time_s参考节点，逐一验证原站点完整覆盖和当前原输入位置；既有普通无参考路径不变。末轮连接图迭代不再生成不会被下一轮使用的候选。Release/Debug主程序和Native构建、各四项领域回归通过；原输入不符/参考文件篡改/遗漏原始站点三项真实引擎拒绝测试通过。42SDK DLL/配置SHA不变，CRLF正确，正常CMake注入已恢复。

外部prepare_attitude_reference.py从旧已验收实际runtime关节曲线和原源时间构造有序参考，动态规划对应2000个完全相同位置和400个开口延伸站点。生成数据与运行数据字节完全一致。finish_gui_run.py仅在真实GUI完成、独立连续/密验收及真实MainWindow播放全通过后复制参考数据和profile到交付场景及任务输入，尚未执行激活。

当前Native29040与原MainWindow5584均用冻结ryh1-reference-knots-bin从原2400点/全局Top1运行，4360/4472参考节点通过，仍在端部姿态与连接搜索；首次连接探查132不合格，不等同132实体碰撞。watcher39136仅等待本次GUI报告后严格验收；无整条final，旧ryh不变。外部状态为output/current_execution_status.json。


### ryh1 端部横向偏移通道试验（尚无整条验收）

独立网格核查已证实旧姿态延伸时长喷枪中段与内壁真实相交，SDK约949mm穿深仅粗值，不作为实际穿入量。固定旧姿态的横向YZ提议覆盖全部200个远端新增离散站点，但最初probe的工艺参考随位置偏移，不能据此声称原喷涂工艺通过。新probe显式提供未移动原点作工艺参考：保留2400原站点和旧曲线节点，并在延伸边每10mm插值，10722节点全部IK有效，7碰撞、540工艺未过，最大相邻关节步14.145度；均为失败候选诊断，不是final。既有服务重验证10400/10722通过。

外部prepare_transverse_reference.py以原轨迹有序对应、端部候选小图和sin平方渐变构造可重复的参考提议；共享SprayGuidedRepair仅对未通过的参考节点，先尝试5至95mm的横向小偏移，保持原参考姿态/turn并重新执行实际IK、喷向、限位、位置与完整碰撞门禁，再回退原姿态族。无公开API、依赖或SDK变更。隔离reference-transverse Release/Debug构建验证待完成，旧二进制冻结继续运行，默认活动任务与旧ryh final未改。必须整条真实MainWindow、APF/单轮QP、独立连续及高密度验收通过才发布。


### 2026-10-17 新显式 STL 场景运行及检测器兼容（整轨迹未完成）

本次输入为 ABB4600-burnner.sys.json（SHA b0b303aab044）和当前 ryh1.txt（SHA 86f5af28c9d6），独立对象 A 保留原毫米缩放0.001及原变换；未复用旧工件URDF、旧安全证书或旧姿态参考。2400点全IK找到19200候选，全局Top1代价523.623345046679。原窗口首次因UI硬编码 obstacleId=burnner 及 robotId target 而提前拒绝，Native完成原站点姿态搜索但在可选加密点2.659053s无安全姿态提前退出；均不是最终成功。

共享ProjectCdfQpTrajectoryRepairService新增只读isCollisionSetupConfigured查询，与ensureCollisionSetup共用工件解析，支持显式RobotObject检测器；MotionPlanningModuleController移除三个重复局部检测函数，初始化、CDF按钮和播放统一使用共享查询。旧RobotRobot保持兼容。新增对象检测器就绪、禁用、缺target、错误机器人及旧机器人型工件回归。SprayGuidedRepair仅移除搜索不到安全姿态的本轮可选加密探针，原始站点始终保留；连接仍需后续APF和完整连续检测通过。未放宽碰撞/工艺门禁。

Release和Debug主程序、Native及四项领域回归均通过。一次测试target拼写错误（缺连字符）已更正，不影响主程序构建。外部launch_scene_workflow支持有独立证据名的attempt；v2原窗口39716、Native27176已重新计算。当前无最终无碰撞结论，runtime.json/active_ryh_job.json未激活，旧结果不动。隔离CMake注入待运行完成后恢复；完整导出、独立密验证及真实MainWindow播放仍待完成。


### 2026-10-17 新场景连接连续性与拐角前瞻（v3，完整验收中）

v2全轨迹尚未到完整APF阶段，发现post-Top1喷涂引导对未知可连边施加1e5惩罚，导致>60度关节步从原始Top1的3处增至219处（>120度143处），不应当将关节直线无法连接等同不可绕行。已主动中止本次v2 GUI39716/Native27176及其watcher，保留证据，未发布final。共享SprayGuidedRepair将该偏好降为可配置guidance_blocked_connection_weight（缺省10）；原全IK分层图完全二部连接未变。

ApfLocalPlanner仅对碰撞前瞻跨过拐角，并将拐角后的首站纳入有限120mm弧长预算；吸引和跟随仍逐站经过并在拐角停止吸引前移。回归覆盖跨角、超预算点不预测及输出原站点不被抄近路。CdfQpTrajectoryRepair对明确允许>=125mm连接范围的喷涂任务优先尝试完整腕部逃逸范围，全部窄范围fallback保留，已有区域成功宽度仍优先；后续轨迹形状恢复、单轮QP及全部硬安全验证不变。

外部任务显式采用planning_configuration_revision=2，并在output/before_planning_profile_revision_2保存上一版profile和metadata。源场景/2400源轨迹/模型/变换/缩放/碰撞过滤均未变；更新后的profile SHA记入metadata，旧运行自身viewer_config_v2与旧profile副本保留。新参数：原站点安全姿态图单次选择，不把困难Cartesian弦逐轮变成大幅换构型；后续连接交APF。以新STL左端截面边界计算开口引导中心[1.2,.6,.62283155]，仅产生经实际IK/碰撞/喷向检查的提议。连接150mm、原站点100mm和轴向2mm覆盖硬门禁保持。参数与prepare_scene新建任务逻辑同步；启动器新增源场景及所有静态输入SHA检查。

最新Release/Debug主程序和Native构建、四项领域回归全部通过；两配置42SDK DLL同既有冻结基线SHA，Debug重新部署的三DLL已恢复基线。原MainWindow45404、Native38972从原输入重新计算；watcher27388仅在当前原窗口四阶段成功后进行独立连续/密碰撞、限位、动态与全源站点覆盖核验和真实MainWindow播放，均通过才激活。当前无完整无碰撞final。早期24点切片仅诊断，均已主动中止且记录aborted，不用于整轨迹验收。隔离CMake注入仍待最后恢复。


### 2026-10-17 原始TCP走廊与安全IK种子分离（v4，184区间连通中）

v3在新150mm授权下实际仅使用79.731886mm局部范围：错误地把全轨迹最大guide偏移70.267114mm从每个局部段统一扣除；且APF位置引导仍贴在移位后的guide上，不是原始路径。已中止v3 GUI45404/Native38972与watcher，未发布final。v3安全引导2400点，相邻>60度7处、>120度2处，相比v2的219/143已显著减少，但不当成最终运动跳变结论。

私有repairCdfQpWithCheckpoint新增可选originalTcpPositions，独立传递每个规划结点按原始源时间插值的位置；原cartesianTargets仍是实际安全种子的完整姿态，原FK一致性验证保留。原始位置逐点数量/有限值/FK缺失均拒绝，回归覆盖不合法数量和NaN。SprayGuidedRepair使用原始位置直接约束150mm连接范围，不再扣全局guide偏移；原站点100mm、轴向2mm、原喷向及全部碰撞门禁不变。APF仅在种子超出原走廊时恢复位置，走廊内已有安全偏移不被强行拉回原碰撞中心线；新增回归验证保留安全offset。无公开API或SDK新增，此处仅私有续算参数扩展。

Release/Debug原主程序及Native构建、各四项领域回归通过。SDK42DLL基线与源CRLF已核对；最终正常CMake配置恢复待执行。原MainWindow25112用ryh1-original-corridor-bin从原ryh1重新全IK/Top1/生产CDF；Native33884仅复用并重新实际验证v3选择的guide，不复用APF/QP或任何碰撞证书，以快速诊断同版后段。当前完整扫描184碰撞/形状不合格区间，首个旧长期失败的56-58及其后两个区间已实际通过swept验证。真实GUI验收watcher38728严格等待当前报告后再完整独立复验与播放，无final/active/runtime.json激活。


### 2026-10-17 新场景 APF 喷向势场尺度诊断（v5 对照，未验收）

v4 Native与原MainWindow都在加密2135-2138附近的转向连接持续搜索，已经通过36个先前局部区间；未进入最终QP或发布final。发现后置喷涂tcpStateClearance用0.1*(cos(theta)-cos(limit))将20度喷向锥的最大软余量限制到6.031mm，而APF影响半径为80mm，因此完全对准仍有约2.31斥力权重；混合势场存在不必要竞争。

SprayGuidedRepair私有helper将每个角度锥的有符号余量按完整合法范围归一到当前APF影响半径；零边界、越界符号、真实喷向admissible、碰撞、位置及全源站点覆盖硬门禁均保持。只修改私有实现/头和SprayGuidanceTests，新增理想对准、边界、越界、合法内部及退化输入回归。Release/Debug与真实新场景对照尚待完成，不能声称已改善或获得整轨迹。v4冻结二进制与进程不覆盖，v5隔离bin为ryh1-angular-potential-bin；默认活动任务和旧已验证结果不变。


v5后续验证：Release/Debug主程序、Native、APF及喷涂测试构建通过；各四项领域回归通过。Debug构建重部署的lib_coacd-D.dll/tbb12_debug.dll已恢复42DLL冻结基线并重新运行Debug四项通过；正常CMake配置曾恢复。真实MainWindow v5 PID21396与Native21796运行中。旧v4原窗口25112及watcher38728主动停止保留记录，Native33884已完整退出为失败：通过37局部段，2135-2138最终在100mm窗口连通，2257-2261各窗口未进入力场迭代，QP轮数0，不能发布。

开始v6投影早退修复对照：ApfLocalPlanner将内部吸引参考的失败投影视为可修复提议而非整条失败；投影在副本上执行，失败/非有限/越限副本丢弃，原有限参考保留。固定端点、实际输出的全程碰撞和工艺/位置硬门禁不变。私有oracle补充未投影参考数量日志；APF测试新增故意使碰撞参考投影失败且污染副本NaN的案例，要求仍找到无碰撞绕行且端点、点数和工艺门禁不变。v6构建/实测尚待完成。v5冻结版本不覆盖；v5后台额外从v4部分路径续算只作诊断，门禁先核对重新生成的全参考与全部q/时间/限位/端点，再重新检查运动，不继承安全证书。


v6验证进展：Release/Debug主程序、Native和APF/喷涂回归目标构建通过，各配置四项领域回归全部通过。两配置42SDK DLL已恢复旧冻结SHA基线，CRLF/源hash核对成功；正常CMake配置已恢复（CMAKE_PROJECT_INCLUDE移除，Copy_RuntimeFor_RobotQtViewer=ON）。v5仅改软角度势场的续算诊断再次在2257-2261未进入力场即失败，0轮QP，原窗口21396、重复Native21796和watcher34144已主动停止且留证；v5续算Native39256自然失败退出，未发布。

v6真实MainWindow35584从原输入重新全逆解/Top1/生产CDF；Native16520用v4部分APF作为诊断，已通过重新生成参考与q/时间/限位/端点一致性门禁，正在重查全部运动/工艺。watcher32940只接受v6真实窗口成功四阶段导出后，独立连续/密检测、原站点实际FK覆盖、动态限制与真实窗口播放均通过才发布。尚无新场景最终无碰撞轨迹，runtime.json与active_ryh_job.json未激活。


### 新场景APF两阶段搜索顺序（v7实验，整轨迹未验收）

v6续算实测已通过旧失败2257-2261和后续2381-2388；日志显示10个投影失败内部参考并未阻断实际APF，修复后实际连接通过完整门禁。当前在2510-2514耗时，多组宽走廊场/锚点仍搜索，真实MainWindow35584独立前进；不是整条成功。

为减少单个宽走廊中先耗尽所有大幅逃逸场的等待，私有ApfFieldStage分All/Basic/Escape，通用APF缺省All保持原次序；喷涂repair先在所有原走廊/原锚点组合尝试原14个普通方向场，再在同样组合尝试全部原几何/大角度逃逸场。未删除任何原搜索场、未改各场迭代预算、端点/碰撞/工艺/位置/原站点门禁。明确>=125mm范围的宽度顺序改为由宽到窄，区域已成功宽度仍最先，其余较宽/较窄fallback都保留。增加阶段回归：Basic不能偷用窄通道几何种子，Escape仍可通过该通道并接受逐边运动核验。v7构建和完整实际效果尚待验证；v6冻结运行不覆盖。


v7对照选择：v5/v6角度标度归一试验未表现稳定改善，前部某些连接需更多方向场；恢复before_angular_potential_v5内SprayGuidedRepair.h/.cpp及SprayGuidanceTests.cpp，退役试验源存output/retired_angular_potential_v5。保留实测有效的v6投影副本/fallback修复和v7完整搜索集的两阶段次序。copy2保留旧mtime的首次恢复没有触发重编译，未运行该中间二进制；已显式刷新三源mtime、实际重编译并核对日志。最终Release构建及四项领域回归通过，v7原MainWindow37076/Native34524已启动（Native只用v4 partial作完整复验后的诊断），Debug构建待完成。v6两进程保留对照，停止watcher32940防止重复发布；当前仅v7 watcher19520允许触发严格验收。无最终新场景无碰撞结论。


v7最终构建核对：原角度势场版本的Release/Debug主程序、Native及两受影响测试目标均构建通过，各配置四项精确匹配领域回归全部通过。42SDK DLL均同原冻结SHA，Debug重部署的两个DLL已恢复；七个相关源/头/测试CRLF与hash核对通过。正常CMake配置恢复完成，未留注入。v7实际运行继续，无整条APF或QP成功，未发布final。更正一次用户进度口误：v6日志出现参考节点2632是扩大锚点的内部节点，并非2510段通过；已向用户明确更正，统计仅以APF interval verified为准。当前v6在2510段继续搜索，只有2257和2381两处新增连接实际通过。


### 2026-10-17 连续关节 APF 大幅逃逸搜索范围（v8，整条尚未验收）

v6已实际通过2257与2381连接，v7仍在2510局部搜索。核对发现全部continuous关节最后一级局部范围仅参考角上下0.75rad，启用的1.10/1.65rad场会被截断；有限硬限位关节最后一级原已使用硬全范围。仅在喷涂引导且TCP连接授权>=125mm的最后一级，将continuous搜索padding扩大至1.75rad；实际模型硬限位、TCP150mm、原站点100mm与轴向2mm、喷向和完整碰撞门禁均未放宽。保留v6失败参考投影副本隔离/fallback修复；撤回未证实有效的v7两阶段调度与角度势场归一试验，原场集和顺序恢复。

新增窄通道回归：0.75rad上限必须失败，1.75rad必须以固定端点通过逐边oracle检查。Release主程序、Native及受影响测试目标构建通过，APF等四项回归通过；42DLL与冻结基线SHA一致、源CRLF和hash核对通过。Debug构建及实际v8场景运行待完成，尚无整条无碰撞轨迹，未激活final或旧活动任务。


v8后续验证：Release/Debug主程序、Native及受影响测试构建全部通过，两配置四项领域回归全部通过；42SDK DLL均核对冻结SHA，Debug两个重部署DLL恢复后再测试通过。正常CMake配置恢复完成。v8真实MainWindow46536重新原输入全IK/Top1/CDF，Native27340仅以完整复验后的v4部分几何诊断，唯一严格watcher26136等待实际窗口四阶段结果后再独立验收。v6/v7旧计算保留对照；v7 watcher19520已停止防重复发布。当前尚无完整APF/QP成功。


### 2026-10-17 APF 原参考 TCP 横向平滑候选（v9 试验，未验收）

v8连续关节范围修复已构建并通过两配置回归，实际Native新增通过2257连接，但仍在2381连接搜索；原MainWindow独立运行到2135附近。未将搜索中间状态或内部节点编号当作连通成功。旧ryh最终轨迹在本次新场景抽查3432节点，843碰撞或无效；同STL几何浮点缩放误差约3e-8m，但本次工件Pitch=pi，旧为0，旧证书不可沿用。

为避免仅扰动关节导致长TCP不受控的大偏移，私有ApfSeedFields新增可选generatePhased回调，提供实际弧长phase及当前参考q，原三参接口保留。喷涂领域增加14组TCP横向候选，幅度25/50/95mm、sin平方进退、以当前参考实际FK为基准，保持轴向位置并限制提议总偏移；实际IK/turn/限位/喷向重新验证，旧18入口姿态族和全部原关节场保留。只改变吸引提议，不接受未经完整碰撞/工艺/走廊检查的边或路径。公开API、SDK、依赖未改。

新增回归覆盖分相位窄通道、正反方向物理phase、固定端点、逐边oracle及越限提议拒绝。四个领域私有源/头/测试使用CRLF，冻结v8运行未覆盖。v9构建/实际效果待验证，bin为ryh1-transverse-pulse-bin；无最终结果发布。


v9最终构建核对：Release/Debug主程序、Native与受影响测试目标全部构建通过，各配置四项精确匹配领域回归通过；42SDK DLL均同冻结SHA，Debug重部署的lib_coacd-D.dll和tbb12_debug.dll恢复后测试通过。源CRLF/hash核对通过，正常CMake配置恢复。真实MainWindow27840从原ryh1/当前指定场景重算全IK、Top1和APF/CDF；Native5960只用本任务v4部分路径、并完整重查作为对照诊断。唯一验收watcher45036等待v9原窗口四阶段及独立证书/播放通过才发布。v8 GUI46536与Native27340保留比较，v8 watcher26136已停止，v7 Native34524仍为诊断。当前无整条结果，不激活runtime.json或旧活动任务。


### 2026-10-17 可选APF几何场单点提议失败不取消整场（v11，待验收）

v9实际日志中2381附近第一局部窗口所有新增14横向场及18入口姿态场在力场迭代前均被取消；某一内部站点未能生成姿态提议，不能等同整段不可连。新增私有allowPartial opt-in（缺省false），仅喷涂几何场启用：逐点提议在副本上生成，失败/NaN/越限副本丢弃，该点继续使用已核对有限/限位的原吸引参考；其它有效点继续；整组没有任何有效新提议则跳过。输出点数、固定端点、完整运动/工艺/位置门禁不变，没有把失败状态当作安全状态。新增partialSeedField日志分清提议失败和实际运动失败。

新增回归覆盖部分提议失败并污染NaN、缺省整场取消、opt-in可通过窄通道、全部失败跳过及越限值不进oracle。只改五个领域私有源/头/测试，无公开API/SDK/依赖变化。v11隔离bin=ryh1-partial-field-bin，当前构建/实场待验证；v9冻结运行及guide3参数对照不覆盖，没有新最终结果。


v11构建与启动核对：Release/Debug主程序、Native与受影响测试目标均构建成功，两配置四项精确匹配领域回归全部通过。42SDK DLL冻结基线SHA一致，Debug重部署两个DLL已恢复；源CRLF/SHA核对通过。正常CMake已恢复（移除CMAKE_PROJECT_INCLUDE，Copy_RuntimeFor_RobotQtViewer=ON）。真实MainWindow42780从原输入重新全IK/Top1/CDF；Native30336仅作重新完整核验后的v4部分路径诊断；唯一严格watcher31156等待完整GUI与独立验收。v8对照已主动停止并记录retired_v8_processes_v11.json。尚无整条无碰撞final，不激活旧任务。



2026-10-17 新场景后置开口姿态连续提议诊断：v11局部停滞94状态单点均无碰撞，15状态对对应原参考喷向不合格，尚非整条成功；v9重复对照已主动停止。guide3参数对照完成：同2400源点，最大guide关节步由178.15降至48.60度、最大TCP偏移43.27mm，但仍有105个Cartesian连接未通过。独立开口提议实验3180个中3150单点安全，不视作连接证书。
基于当前新场景guide3（不是旧ryh最终轨迹）建立开口参考试验：对开口外节点使用两侧内区guide姿态及横向位置平滑延续，保留原x/全部2400站点、位移<=99.5mm；1592提议核验后替换398个外区节点，首末节点不变。新不可变job为原job名加_rim_reference；数据用已有attitude_reference接口仅作吸引提议，生产服务重新核对实际IK/碰撞/工艺/覆盖，没有复用安全证书，原全IK/Top1图不变。没有新增C++/SDK/依赖改动。原MainWindow46864和Native5796均从原输入重新全流程运行，严格watcher已启动、旧watcher31156停止，尚无final。



2026-10-17 当前场景开口参考DP试验：389外区提议经连续性选择，原2400源点全部保留、最大TCP偏移49.29mm、参考最大关节步48.60度。发现生产载入后末点却跳至172.89度：attitude_reference分支无条件用原Top1末端关节覆盖参考，虽然参考末端是相同TCP的另一有效IK分支。用户已允许相同起终位姿的不同逆解。拟在SprayGuidedRepair私有端点选择函数增加profile opt-in attitude_reference_equivalent_endpoints（缺省false兼容旧行为）；只有严格实际FK位置/姿态、有限值和硬限位均通过才保留参考分支/turn，完整safeCandidate及运动/工艺/覆盖/独立验收不变。新增非周期镜像构型、合法多圈、默认分支、非法位姿/角度/限位/NaN/维度回归。三源备份与SHA存_rim_dp/output，v14待构建验收。
外部启动脚本增加--job以及严格快照选择helper，避免发布/再次启动时丢失当前不可变profile变体而误回原profile。匹配输入/自动场景、错误源/目录越界/篡改profile五项校验通过。旧active不变。



v14端点参考修复验证：Release/Debug主程序、Native和受影响测试目标构建成功；各配置四项精确领域回归全通过。两配置42SDK DLL同v11冻结基线；Debug重部署的lib_coacd-D.dll/tbb12_debug.dll恢复后测试通过。正常CMake已恢复。原实际FK核对参考首末点误差分别约1.6e-12m/7.5e-7m、姿态1.1e-12rad/2.2e-7rad，均严格满足原容差。新实际guide最大关节步48.60度，原无条件覆盖导致的172.89度末点突变消失。
当前唯一GUI验收job为原job名加_rim_reference_endpoints；398个开口外点使用当前场景内区姿态平滑提议，2400点及原轴向范围完整、最大guide位移51.68mm。原窗口17440、Native24864运行全流程；唯一watcher45652等待真实四阶段导出后完整独立验证。旧v13 GUI/Native/watcher与guide3 Native已主动停止，记录retired_prior_trials_v14.json；rim12 Native5796仅保留前缀诊断对照。没有完整无碰撞final，旧active未改。


### 2026-10-17 v14开口连接与后置参考连接细化对照（未验收）
真实MainWindow17440继续指定ABB4600-burnner场景与原ryh1的完整全IK/原全局Top1/APF/CDF，36处局部修复已验证，2271-2274加密索引连接仍在扩大锚点搜索；该计数不是剩余碰撞数，没有完整最终轨迹或新的安全证书。旧rim12 Native5796只保留前缀诊断，不可发布。
为定位单轮后置参考选择的连接瓶颈，新建独立不可变job _rim_reference_endpoints_connections3，仅把已有guidance_connection_passes由1改3；原Top1、2400源站点、全部静态几何/碰撞/喷向/100mm源站点及150mm连接范围均不变，无C++/SDK/依赖更改。Native10908仅诊断；第一轮未连接68，第二轮70，尚不能认定细化改善。原GUI与唯一watcher45652继续，ACTIVE不变。输出live_progress.json明确区分局部修复、完整工作流和最终认证，不把中间路径当交付结果。

### 2026-10-17 双向APF已验证前沿拼接（v18，构建/实际验收中）
当前v14原GUI在2271-2274仍搜索，未发布完整结果。三轮后置连接参考试验v15单点未连接69（原68），但Native已实际通过2304-2306一处同类困难连接并到2432-2434；原MainWindow18868从原输入/全IK/全局Top1重新跑该配置，独立watcher27556尚未验收。现有两项输出均不可当整条无碰撞。固定参考位置的姿态平滑v16共7194提议，7137安全新候选，DP采用2124，初始shape/碰撞区间168降到155；它仍是Native诊断。旧v11前缀Native5796已验证身份后主动停止，记录retired_prefix_native12.json。
200个实际日志停滞状态以当前实际FK和碰撞器独立抽查，碰撞/IK无效0，喷向不合格0；61度与85度门禁均有效，故没有采用放宽喷向限制。瓶颈是连接而非这些状态本身。代码核对发现既有正反向场失败时丢弃已验证前缀。v18在私有APF保留正向prefix和原顺序reverse suffix，只在覆盖重叠/相接、原始turn差<=0.35rad的短连接中按代价选最多16个接缝；每个接缝与拼成的每一条边都重新检查原guide/工艺和完整运动oracle，保留原点数、锚点、原字段全搜索，无wrap、无删碰撞边、无跳过原站点。仅有clearance引导的路径启用；普通APF原流程保持。
修改ApfLocalPlanner.h/.cpp、CdfQpTrajectoryRepair.cpp与现有ApfLocalPlannerTests.cpp四个私有文件，CRLF、事前备份/源SHA已记录。新增前后半场互补的完整规划回归，拒绝坏接缝/无覆盖/NaN/越限/错误turn/被拒运动，验证逐边完整重查。公开API/SDK/依赖未改。隔离v18 bin构建中，不覆盖当前已运行v14；最终仍以真实MainWindow四阶段导出及独立密检/全部2400源站点覆盖和播放通过为发布条件。

v18最终构建核对：新增互补前后场规划回归最初使用vector精确浮点比较而返回157，改为1e-12数值误差检查，保留必须触发stitched回调及逐边oracle的条件后通过；算法无需为此放宽。额外拼接碰撞尝试最终限制为每个新场总计16次（不是每对16次），先按原turn短接缝代价排序，完整拼后逐边检查不受次数预算截断。Release/Debug主程序、Native及受影响测试均构建成功，两配置四项领域回归全部通过，SDK全部DLL与v14冻结基线相同，正常CMake已恢复。证据build_bidirectional_apf18_bounded.log、ctest_bidirectional18_*.log、bidirectional_apf18_final_source_hashes.json与事前备份在v14 job/output。
新不可变job为_rim_reference_endpoints_bidirectional，全部输入/profile/几何同v14，运行库为ryh1-bidirectional-apf-bin。真实MainWindow31200从原ryh1重新全IK/原Top1/生产APF/CDF，Native13940仅诊断，唯一严格验收watcher22760。旧watcher45652、27556已停止且身份/命令留档retired_old_finishers18.json，避免旧对照自动发布；旧GUI/Native暂作比较。尚无本场景完整无碰撞轨迹，ACTIVE仍未改变。

v18真实实场进展：原MainWindow31200在2151-2153区间通过bidirectional stitch（field2，正向9站/反向6站，全边复核），该区间402.363到409.115秒，约6.75秒；v14同区间390.593到686.000约295.41秒。该局部加速不能当整条成功。当前在2271-2274扩大锚点继续搜索。冗余Native13940已验证进程命令后主动停止，留档retired_redundant_native18.json，最终独立验证仍必须执行。
为结合互补进展，新不可变job _g3stitch使用v15三轮后置调姿实际选中的2400源构型作为几何提议，原始ryh1/scene、原全IK全局Top1、FK、限位、喷向及100/150mm范围不变，引用文件哈希和static_input_sha256均重建。该参考曾通过v14困难连接，但不是完整无碰撞路径，未复用证书。v19真实MainWindow37768从原输入重新全流程验证，bin仍为已双配置测试的v18；配置C:/b/rs105-merge/ryh1-cfg19。唯一自动验收watcher42784，旧v18 watcher22760停止但原GUI仍为对照，记录retired_old_finisher19.json。v19负责最终严格独立碰撞/2400站点覆盖/播放；未发布final，ACTIVE未变。

### 2026-10-18 现有自适应瓶颈细化参数对照（v20，未验收）
v19原MainWindow37768仍在2432-2434局部连接搜索，已38处局部修复verified，不代表整条成功。核对现有SprayGuidedRepair：guidance_adaptive_passes=0时不在长连接失败位置新增中间姿态候选，而原输入相邻位置中位距离约91.07mm。新建不可变job _g3stitch_adaptive20，只启用既有4轮自适应细化及12轮连接选择；原输入、全IK/全局Top1、场景、全原始2400站点、100/150mm范围和完整安全门禁均不变，未改C++/SDK/依赖。Native4052只作独立诊断，不能代替真实MainWindow验收，不复用安全证书。第一轮69处瓶颈补点，1个可选中间提议无安全姿态已交回APF，原源站点无裁剪。当前无新最终无碰撞结果；唯一发布watcher42784仍只等v19真实GUI，ACTIVE不变。复现实验脚本在v19/output/launch_adaptive20.py。

### 2026-10-18 APF双向前沿小缺口拼接（v22，已双配置回归，实场未验收）
v19在2432-2434长时间停滞，未取得完整APF/QP。检查现有双向拼接仅接受前沿覆盖重叠/相接，少数中间加密站点缺失时未尝试连接。仅改私有ApfLocalPlanner.cpp/.h和既有ApfLocalPlannerTests.cpp：最多8个缺失站点用原始关节坐标插值作提议，仍限制前沿关节总差<=0.35rad、不wrap、不改点数/两端锚点；每条补边及拼后完整全边重新检查原guide/喷向/位置/运动碰撞oracle，共享每新场16次接缝预算不变。原APF搜索集、场次和安全范围保留；公开API/SDK/依赖未变。增加缺口辅助函数和真实planApfPath缺2站点成功回归，以及碰撞接缝、错误turn、越限、NaN、过大缺口拒绝测试。
首次v21 Release通过但Debug捕获suffix.begin()+resume+1-suffixBegin中间越界迭代器，已停止该测试及首次主窗口试运行，改为suffix.begin()+(resume+1-suffixBegin)，没有接受其结果。v22 Release/Debug主程序、Native及受影响测试均构建通过，各四项领域回归通过（Debug APF约12.46秒），冻结SDK DLL哈希与v18基线一致；CMake已恢复正常注入关闭和Copy_RuntimeFor_RobotQtViewer=ON。三私有源CRLF核对、事前备份、独立diff APF_bounded_gap22_change.patch及源hash均在v19/output。v21隔离bin和失败日志仅归档，不能作为运行发布库。
正式实场新job _g3stitch_gap22，bin=ryh1-gap-frontier-fixed-bin，真实MainWindow21232从原输入全IK/全局Top1重跑；唯一严格验收watcher4480。旧v19 GUI37768已验证身份后主动停止，日志保留，不是自然失败；旧watcher42784和v21 GUI44276/watcher27380均停止。Native4052的既有自适应4轮/12轮参数对照仍为诊断，第一至第六轮未连接69/134/147/156/157/152，不能当改善或最终结果。ACTIVE未改，没有新完整无碰撞final。

v20诊断结论：四轮自适应后2898参考点，全部2400源点保留；第11/12轮均138未连接，原站点最大TCP偏移仍49.291772mm，但最大原结点关节步升至179.99256度，劣于v19/v22的48.595535度。因此未采用此参考，Native4052在guide checkpoint完成后主动停止，retired_unhelpful_adaptive20.json与comparison_decision.json留档，不进入其APF、不发布。当前仅v22真实MainWindow21232和严格watcher4480在运行；v22已21处局部连接verified，未完成整程。

### 2026-10-18 小缺口工艺投影v25与授权大幅调姿v26/v27（实场尚未验收）
v22实际仍停滞2432-2434，不能宣称修复；38处局部verified不是整条安全。相同已调姿TCP的全逆解分支诊断（postguide_branch_probe23）19200候选/2400点，直接[-pi,pi]典型区间结果有177.29度跳变；按实际URDF六joint均continuous的特殊模型做显式turn提升，代价118.46到118.22，最大步48.60到48.66度，没有实质改善，未采用。FK同姿态误差<1e-6m/rad，2400节点无碰撞仅是单点诊断，不是运动证书，也不改变原始输入全IK/Top1图。
v25仅三个APF私有源/头/测试：原始小缺口插值优先，失败时可对缺失内部站点使用既有喷向/源点覆盖投影；固定前后前沿不动。投影失败/NaN/越限/原始角差改变>.35rad直接拒绝，原raw turn保留，每次raw/投影尝试共享原16次预算；补边和整条全边完整验证。旧重叠前沿优先，防止新增缺口预算抢占旧成功接缝。新增投影恢复流形、污染副本/错误turn拒绝、预算耗尽不得调用投影等回归。Release/Debug主程序、Native和相关测试构建成功，各四项领域回归通过，全部冻结SDK DLL hash保留；正常CMake已恢复ON且无注入。隔离bin=ryh1-projected-gap-bin，源hash与备份before_projected_gap25、build_projected25*.log在v19/output。无公开API/SDK/依赖改变。
重新核对用户已授权“允许大幅调整姿态，只要仍能喷涂内表面且位置尽量不变”：原inner_surface的max_surface_target_angle_deg=20还限制相对指定参考落点方向，单独放宽到60做不可变job _g3stitch_wide_aim26；实际命中内表面、原61度入射角、全部2400原站点、100/150mm位置范围、碰撞模型/场景变换不变，相关依赖FNV和静态SHA重建。原69未连接降至14（同49.291772mm参考最大偏移、48.595535度最大关节步），这是参考连接预检查改善，不是最终安全。Native27584仅诊断，原全IK/全局Top1保持不变。
新正式job _g3stitch_wide27结合v25库和60度授权调姿配置，真实MainWindow29556从原ryh1/指定场景重新全流程，唯一严格watcher5080。v22旧GUI21232/watcher4480核对身份后停止，记录retired_for_authorized_wide27.json；不是自然失败，未发布结果。另发现被旧交接误记已停止的v16孤儿Native34028仍运行2小时以上，核对完整命令属于_smooth_reference且仍在2226附近后已停止，retired_orphan_native16_34028.json留档。用户独立运行的RSW GUI29136未操作。当前ACTIVE仍旧验收任务，未生成本次完整无碰撞final。


### 2026-10-18 v27 实场连接进展与现有连接候选诊断 v29（未验收）
v27真实MainWindow29556初检35个碰撞/形状区间，明确12处局部修复verified；现停中段加密索引6187-6204继续完整搜索，节点索引不是原始站点进度。此后尚无QP或整条成功。抽查59个日志停滞状态（日志舍入角度）实际状态均无碰撞/越限，其中1个以自身TCP为参考的工艺检查未通过；该诊断没有正确的原参考对应，不能用作完整连接/工艺证书。证据在v27/output/stall_diagnostic_28。
冗余v26 Native27584实际推进到与原窗口相同区域，核对完整进程命令后主动停止，留档retired_redundant_native26.json；不采用其路径，原窗口、唯一watcher5080及用户窗口29136未操作。
新建不可变v27_connections29诊断，仅guidance_connection_passes由1改3，guidance_adaptive_passes仍0；启用已有连接失败附近的pitch/azimuth/position候选扩展，原全IK/全局Top1与全部2400源点、场景/几何、60度落点方向/61度入射、100/150mm和碰撞门禁均不变。只生成调姿guide checkpoint后主动停止，避免重复APF；不是最终结果，不发布或改变ACTIVE。配置和静态hash经select_explicit_job核对。没有新增C++/公开API/SDK/依赖改动。
v25相对v18的完整三个私有文件补丁已存v19/output/APF_projected_bounded_gap25_complete.patch，源CRLF/hash再次核对。旧v22局部补丁保留，不冒充包含v25。


### 2026-10-18 低余量喷涂APF近距离排斥补充场 v30（构建/验收中）
v27同一中段仍在第二组锚点搜索，12处实际verified，未QP。核对APF原80mm排斥下限与本次0.1mm软余量差异，在狭窄工艺走廊可能令排斥与位置跟踪互相平衡；这是待实场验证的因素，不是已证明根因。
仅改领域私有ApfLocalPlanner.cpp/.h、CdfQpTrajectoryRepair.cpp和既有ApfLocalPlannerTests.cpp：在具有工艺clearance且0<配置余量<=1mm时，新增一组近距离普通双向/切向场，作用半径max(5mm,5*余量)，之后保留全部原普通、几何和大角度场。其余规划原次序/半径不变。只改变势场力的提议，硬碰撞、工艺、原站点、走廊、限位和固定端点门禁不变。日志增加实际势场作用半径，与验收安全距离明确区分。新场纳入既有已验证前沿拼接，拼接仍全边重验。
新增功能回归：圆障碍和其上方5mm窄缝，在0.1mm余量下生成同点数/固定端点/原turn路径，逐段密采无碰撞；oracle拒绝所有运动时不得返回路径。事前四源备份、CRLF和hash在v27/output/before_short_range30。独立Release/Debug bin=ryh1-short-range-bin，保留v25正式运行不覆盖；构建和测试尚未完成，没有新最终路径或证书。无公开API/SDK/依赖变动。


v30后续核对：Release/Debug主程序、Native和受影响测试目标均构建成功，两配置四项领域回归全部通过（Debug APF等总39.08秒）；新窄缝正例、拒绝运动反例通过。冻结SDK DLL哈希两配置与v25全部一致，四源CRLF核对通过，差异存v27/output/APF_short_range30_change.patch。正常CMake恢复流程执行中，独立bin=ryh1-short-range-bin。
v29三轮参数诊断完成，仍14未连接，2400点最大参考偏移49.29mm、最大步48.60度未改善；只有10个站点选择改变（最大39.25度），未采用，guide checkpoint后自动停止，未APF/未发布。证据comparison_decision.json。
v30真实MainWindow10820在Release四项通过后启动，Debug同时完成；唯一严格watcher现6636（原37996的构建报告等待路径错误，已停止更正，未验收或发布任何结果）。新正式job为v27名加_short30；配置ryh1-cfg30，所有输入profile同v27不变。旧v27原窗口29556及watcher5080核对命令后主动停止，retired_for_short30.json留档；并非自然失败或安全结果，用户窗口29136未操作。当前已完成原始全IK/原Top1及2400点Cartesian continuation，初始12330加密点正在全程扫描；未有完整APF/QP结果，ACTIVE仍未变。


v30实际只增加普通方向场的5mm尝试，普通14场仍未通过同一中段，后续几何/大角度场又使用80mm，尚不能检验对长喷枪入口姿态族的假设。v31仅进一步将同一5mm补充范围覆盖全部原方向/几何/大角度场，完整原80mm全场仍在其后保留；所有状态/运动/工艺/源站点/锚点门禁不变。只再改ApfLocalPlanner.cpp一行预算和注释，事前备份ApfLocalPlanner_before_all_short31.cpp，四源完整差异存v27/output/APF_all_short31_change.patch。
v31 Release主程序、Native和受影响测试构建成功，四项领域回归全部通过（2.70秒）；Debug构建与四项待结束。独立bin=ryh1-all-short-bin；新真实MainWindow12100重做原输入/原全IK/全局Top1，正式job为v27名加_allshort31，配置ryh1-cfg31，唯一严格watcher31036同样等待两配置构建标记及完整GUI结果才验收。v30旧窗口10820/watcher6636核对身份后主动停止并留档retired_for_allshort31.json；不是自然失败或安全结果。当前未有最终APF/QP结果、未发布final或更改ACTIVE，用户窗口29136不动。


v31最终构建核对：Release/Debug主程序、Native及相关测试均构建通过，两配置各四项领域回归全部通过（Release2.70秒，Debug46.47秒）。两配置全部冻结SDK DLL与v30基线一致；正常CMake已恢复无CMAKE_PROJECT_INCLUDE且Copy_RuntimeFor_RobotQtViewer=ON。当前唯一正式MainWindow12100与watcher31036继续相同原输入/场景/profile重算，仍在Cartesian continuation；最终碰撞/工艺/覆盖/程序和播放验收尚未发生，不能当已成功路径。


### 2026-10-18 低余量大角度避障先尝试宽锚点窗口 v32（实际验收中）
v31几何场也使用5mm排斥后，窄关节范围的首组锚点仍在中段6187-6204停滞，不能宣称力场半径已解决。检查发现原先60/120/240mm提前锚点分别只开放0.25/0.45/1.75rad连续关节padding，大角度场要等最后一级才真正开放。v32仅在明确>=125mm连接走廊且APF总余量<=1mm的喷涂模式，把原六个窗口的scale顺序由0,1,2,0,1,2改2,0,1,2,0,1；全部原physical/knot窗口保留，所有碰撞/工艺/源站点/走廊/限位门禁不变。仅再改CdfQpTrajectoryRepair.cpp及注释，无新增API/SDK/依赖；事前备份CdfQpTrajectoryRepair_before_anchor32.cpp，四源完整差异APF_anchor_first32_change.patch。
v32 Release主程序、Native与相关测试构建成功，四项领域回归全部通过（2.69秒）；Debug构建测试继续。独立bin=ryh1-anchor-first-bin，真实MainWindow44304从原ryh1/指定场景重算原全IK/全局Top1/CDF；job为v27名加_anchor32，配置ryh1-cfg32，唯一严格watcher39084等待两配置完成及真实GUI全部阶段后才验收。v31旧窗口12100/watcher31036核对身份后主动停止并留档retired_for_anchor32.json；未采用不完整路径。当前无最终无碰撞结果，ACTIVE不变，用户窗口29136未操作。


### 2026-10-18 v32双配置完成与开口内部调姿通道v39（实场未验收）
v32 Release/Debug主程序、Native和受影响测试均已构建成功，两配置四项领域回归全通过（2.69秒/49.01秒），全部冻结SDK DLL哈希不变，正常CMake恢复完成。宽锚点先行仍在6187-6204中段停滞，不能宣称修复；2026-10-18 02:33核对完整进程命令后主动停止原GUI44304/watcher39084，留档retired_for_channel39.json。不是自然失败，不是安全结果。
本轮没有再改C++、SDK、公共API或依赖。外部诊断在同一新场景实际FK/IK/完整RobotObject Exact检测器上进行：保持位置的姿态平滑609提议/520单点安全，相邻最大关节步48.60降33.61度，但抽样仍6条连接碰撞，未采用为最终路径。进一步比较双向倾角1680提议/656安全、缩短喷距1008/507安全，以及8个困难站点的倾角/方位/喷距1200/239安全。均只是状态提议，不是路径证书。
将这些实际单点安全候选作为原Top1之后的几何参考，局部DP按原turn差和小位置偏移代价选取；对所选候选弦仅做0.02rad抽样反馈。该辅助候选图不是原始全IK分层图，不修改原图或碰撞删边。第一轮99个源站点姿态/位置有变，最大相邻关节步24.23度，中部原1210/1214/1234/1238四条直接弦抽样已无碰撞；947/949两条仍需APF（不得把抽样当连续认证）。局部源站点偏移最大95mm，保留全部2400源站点及原起终位姿；原100mm源点/150mm连接、喷线命中内表面/60度参考方向/61度入射、0.1mm软余量等门禁不变。
新不可变job为ryh1_86f5af28c9d6_scene_b0b303aab044_g3stitch_wide27_channel39；新参考输入文件、profile FNV及静态SHA已更新并通过select_explicit_job核对。仍用经过双配置测试的v32隔离运行库ryh1-anchor-first-bin，不复用旧安全证书。真实MainWindow14068从原ryh1与指定ABB4600-burnner场景重新全IK/原全局Top1（523.623345047）/APF/单轮CDF-QP；唯一严格watcher47636等真实四阶段导出及独立完整复验/2400源点覆盖/主窗口播放通过再发布。当前没有完整无碰撞final，ACTIVE未改，用户自己的RSW窗口29136未操作。
诊断/候选选择/启动脚本和所有结果位于v27/output下的barrel_diagnostic33、smooth_diagnostic34、insertion_diagnostic35、inward_diagnostic36、rim_channel37、channel_refinement38及launch_channel39.py。只用于当前输入的安全姿态提议，不把旧ryh结果替代新输入。


v39实际APF阶段进展（2026-10-18 02:46，未最终验收）：真实MainWindow14068全IK/原Top1完成，调姿预检查未连接14降9；最大源点位移95mm、最大关节步24.228793度。Cartesian连续化保留全部2400源构型，2386边连续化、13边交APF、12082规划结点，固定位置调姿50降14次。12193加密点初检30个碰撞/形状区间；328.430秒进入整条APF检查，329.824秒通过门禁并开始安全窗口平滑。源码核对此日志仅在全段碰撞数为0且参考约束通过后出现，因此可确认APF阶段完整门禁已通过，但不能当最终CDF证书。目前第6/14次APF平滑（7种尺度两遍），后续仍单轮QP及最终独立验收。
额外0.0001rad喷向诊断发现原始直接弦1234附近入射角短暂越61度，说明粗采无碰撞不等于喷向合格；v39实际APF已自行处理，未放宽门禁。后续重复候选反馈诊断41已核对身份后主动停止，retired_redundant_diagnostic41.json留档，不采用其未完成候选。当前仅v39主窗口和逻辑唯一watcher47636（Windows venv另有实际Python子进程39404）运行；用户窗口29136未操作。v39/output/live_progress.json由write_live_progress.py生成，便于继续查看，final尚未发布，ACTIVE未变。


### 2026-10-18 v39最终验收与交付完成
实际MainWindow14068原始ryh1/ABB4600-burnner全IK共19200候选，原全局Top1代价523.623345046679，APF/单轮QP/安全平滑/减点完成，主窗口规划3435.110秒。12193加密点减为4221执行点；严格自适应检查发现17段需修补，其中14个平滑脉冲修补覆盖这些段，整路径重新认证。独立实际导出关节角0.0001rad密检2857936状态，碰撞/喷向/限位/离散速度加速度违规全0。最小节点距0.100517989mm，导出舍入扣减后连续间距保守下界0.009889816mm，不代表10mm或实测全程最小值。2400原站点覆盖通过，最大轴向1.996797322mm，站点最大位置99.981077739mm/RMS24.736899146mm，连接最大102.997985730mm，均在授权100/150mm范围。端点位置误差7.595184e-7m；时间参数化时长1027.596019秒。
自动验收首次因watcher默认GBK读取UTF8报告退出，只将该任务watcher改为utf-8-sig后续验。独立验证已成功后，复制GUI配置触发MAX_PATH；没有重算轨迹，任务内resume_verified_channel39.py重新校验输入/引擎/实际GUI导出hash、独立证书和全部站点覆盖，并用C:/b/ryh1-play39短配置继续原窗口录像。原MainWindow播放4221点全部检查，distancePassed/allPointsChecked/finished均true。随后正式发布本job/output/final、runtime.json、completed.json及ACTIVE。cached显式scene/job入口--no-view --no-record复验exit0，未重新计算。
新增Freeform_APF_QP/run_ryh1_burnner.cmd显式查看本次新场景结果，RYH1_BURNNER_RESULT.md说明算法、精确偏差与边界；output/delivery_manifest.json冻结最终文件sha/大小、原输入/场景与当前四私有源hash。当前源hash与v32双配置已构建/各4项领域测试通过时一致，SDK依赖未变；本次仅任务外部验收/启动脚本及文档调整，无新C++/公共API/依赖变化。
限制明确：安全结论仅ABB完整连杆含喷枪对A的关节插补。默认自碰撞器最后帧仍报Link6/Link5重叠，未过滤或声称解决。FANUC LS仅序列化核验，UFRAME2/UTOOL1/MoveL/CNT100未在目标工作站认证，110对点距<2mm、最小0.0273mm仍存在。QP只知执行一轮及五级回退，现有报告无acceptedQpSteps，不把全部平滑改善归因于QP。结果图已实际查看，工件右端仍有局部弯曲，未声称形状完全恢复或全局连续光滑/时间最优。


### 2026-10-19 ryh2 位置优先规划（执行中，未验收）
用户指定桌面ryh2.txt和同ABB4600-burnner场景，轨迹2400点，源SHA32d89774597ded44a7e11d98910550da2189bcedaf339cb7ce1f7d9ba626b50d。原法向姿态与ryh1一致，点位置沿法向约90mm变化，不能复用旧路径/证书。新不可变任务ryh2_32d89774597d_scene_b0b303aab044_pos30：原末端直接全IK19200候选、全局Top1代价510.2817922777992；原图不碰撞删边。后置仅采用旧已验收姿态作为提议，参考位置取真实FK以消除旧位置偏移，所有新位置/姿态重新实际IK及Exact完整喷枪对A检查。提高位置权重到32，源点上限先收紧30mm、连接仍150mm，60度目标方向/61度入射/2mm轴向覆盖/0.1mm软余量保持。
沿用冻结v32已双配置构建及各4项测试通过库，全部运行文件SHA重新校验；无C++/SDK/API/依赖改变。实际MainWindow30604从原ryh2重新全IK/Top1/生产APF单轮QP，唯一watcher33748，独立检验和原窗口播放通过后才发布，ACTIVE仍旧ryh1。新外部脚本在Freeform_APF_QP/ryh2_tools，完整结果尚未产生。验收脚本已提前指定UTF8及短录制配置C:/b/ryh2-play30。

ryh2首次pos30实际MainWindow30604在333.377秒因“原始站点12无安全喷涂姿态”失败，watcher按预期拒绝发布，未覆盖旧final/ACTIVE。原Top1实际节点检查2400点中900节点碰撞；端点各约50.65mm无碰撞。针对第12点891个实际模型IK/完整Exact碰撞/喷向探测中29个单点通过；原位置反向喷向另一内壁可通过，未作为首选；沿横向向内移动40mm并调方位可保持原喷涂一侧。不是整条路径证书，也不证明30mm内不存在可行解。
新不可变pos60任务只将源站点上限从30增60mm（用户已有100mm源点/150mm连接授权），位置权重32、2400源站点和工艺/碰撞硬门禁不变；真实MainWindow24760及唯一watcher20552从原ryh2重新全IK/全局Top1/生产服务。目前仅单点姿态准备，2319/2400旧姿态提议经新位置实际复验通过（663横向修补），尚无完整APF/QP结果。诊断与变体脚本ryh2_tools/probe_station.py、variant.py，未修改C++/SDK/依赖。

2026-10-19 ryh2补充姿态通道：pos60普通候选仍站点12失败，未发布。外部probe_channel.py实际新场景检查31200姿态提议，其中24525单点通过、覆盖2399源站点；station1499补充567提议又获得36个有效状态（包含保持原位置的解）。未采用station12诊断中的azimuth180反向内壁候选。select_channel.py仅对原Top1之后的安全调姿提议按原turn关节差/横向偏移连续性筛选，不改变原全IK图或删其边。新不可变pos60_channel1参考2400点，最大源点偏移50mm、RMS11.0943mm、最大原站点关节步49.5617度；这些仅单点/参考指标。
真实MainWindow42260+唯一watcher32504重新原ryh2全IK/原全局Top1，通道2400/2400复验通过，连接预检9未连接；Cartesian连续化后11266规划点，APF在243.256秒整条门禁通过并开始安全窗口平滑，目前12/14。仍未QP/最终认证/独立密检/播放验收，不发布final/ACTIVE。finish_gui_run.py本任务provenance中的raw ryh1纠正为raw ryh2，仅文字；原运行库/SDK/C++未变。ryh2_tools/deliver.py已准备但必须所有严格验证通过才可运行。

ryh2 pos60_channel1运行继续（MainWindow42260/watcher32504）：原APF整程通过，14次APF安全平滑、单轮QP（五档line search，报告不含accepted步数，不宣称QP本身改善全部形状）、QP后14次平滑、参考形状修整和时间参数化完成。11266规划点经1.5mm/.75deg容差及碰撞检查减到4536点。严格连续认证首遍耗时1717秒，发现53个未通过碰撞/喷向/10微米保守门槛的区间，不能简单称53个实际碰撞；48个安全窗口正弦平方脉冲修补覆盖全部这些区间，10:14第二遍全程重验到1500/4536，尚未最终通过。此处运行明显比旧ryh1慢，不能承诺分钟级结束。实际输出尚无完整final；独立0.0001rad密检、全部2400源站点复验、原窗口播放和导出序列核验仍需自动watcher执行并成功后才发布。
外部8个ryh2_tools脚本语法编译检查通过；本任务没有新C++/SDK/API/依赖修改，旧用户改动保留。deliver.py包含最终证书和原窗口播放硬检查，在所有检查通过前不得运行或创建ryh2_result；run_ryh2_burnner.cmd也由其最后生成，旧run_ryh1_burnner.cmd和旧已验证final不改。新结果目前仍未交付。


### 2026-10-19 ryh2 最终验收与交付完成
任务 ryh2_32d89774597d_scene_b0b303aab044_pos60_channel1 已完成。实际原MainWindow42260从原ryh2的2400个末端站点完成全逆解19200候选，原全连接图全局Top-1代价510.2817922777992；原图不作碰撞删边。后置横向移位与调姿候选、生产APF、单轮CDF/QP、安全平滑与减点完成，11266规划点减至4536执行点。主程序耗时6393.808秒，QP线性求解约8.6秒，主要耗时为安全平滑及连续认证；报告没有acceptedQpSteps，不将全部改善归因于QP。
连续认证首遍53个未通过区间经48次局部正弦平方脉冲修补后，第二遍全程通过。独立实际导出文件密检2736949状态，碰撞/喷向/限位/离散速度与加速度违规全部为0；2400原站点覆盖通过，最大位置偏差59.989840mm、RMS22.270650mm，端点位置/姿态误差2.2032245e-8m/1.8545997e-8rad。节点最小距离0.100157419mm，扣除导出舍入后的连续距离保守下界0.009876940mm。总时间参数化1034.852102秒，最大执行点关节步20.042914度。原MainWindow播放finished/distancePassed/allPointsChecked均true，4536点0碰撞，302可视距离帧无缺失。
完整结果在Freeform_APF_QP/runs/ryh2_32d89774597d_scene_b0b303aab044_pos60_channel1/output/final；便捷交付Freeform_APF_QP/ryh2_result，含关节角TXT、11111格式TCP、FREEFORM.LS、原始/APF/最终分图、距离图及动画。run_ryh2_burnner.cmd显式绑定新输入/场景/任务；缓存入口--no-view --no-record复验exit0，无重新计算。旧ryh1入口及结果保留。
安全结论仅完整ABB4600含长喷枪对A.STL的关节插补；未声明解决原场景Link6/Link5自碰撞重叠。最小余量很小，不代表10mm安全距离。FANUC使用UFRAME2/UTOOL1，序列化已核验，目标控制器编译和直线/CNT插补仍未验收；33对输出点间距低于2mm，最小导出点距0.061327mm。图中仍有局部避障弯曲，不宣称完全贴合、无折角或全局平滑最优。本轮无新C++/SDK/API/依赖修改，沿用既有v32双配置构建及各4项领域回归通过的冻结库；仅外部脚本、配置和文档更新，交付文件SHA核对通过。


### 2026-10-20 默认A.STL场景与删除恢复缺陷（实施中）
默认Qt启动配置切换为ABB4600-Aburnner.sys.json，保留用户已有机器人/工件位姿及轨迹。默认碰撞目标改为现有A对象，RobotObject优先，保留旧检测器ID兼容已有轨迹脚本。发现ensureCollisionSetup在工件缺失时自动push burnner RobotDesc，导致删除另存后在MotionPlanning初始化时复活；在领域服务移除隐式导入，缺失时报错不改变文档。ProjectScene喷涂射线测量扩展为STL对象，沿实际TCP+Z求最近正向三角形命中，应用模型单位和实时对象变换。后续构建/领域回归/原窗口检查及保存重开验证。事前备份C:/b/scene-A-fix-20261020，保留既有工作区修改，不触碰冻结脚本运行库。


### 2026-10-20 默认A.STL场景与删除恢复缺陷（完成验收）
Qt默认启动改为config/projects/ABB4600-Aburnner.sys.json；所给场景的机器人、工件位姿、毫米比例及完整轨迹扩展经JSON逐字段比较保持不变，仅对象sourcePath改为项目内data/Spray420/A.STL并增加ABB4600→A的Exact RobotObject检测器。A.STL从用户桌面同名文件复制，SHA一致。内部检测器ID cdf_abb4600_burnner_detector保留以兼容既有保存/脚本，显示名为CDF ABB4600-A.STL detector。Motion Planning默认目标A，对象优先，播放最近距离使用此独立检测快照。
领域服务ensureCollisionSetup移除缺失工件时自动push burnner RobotDesc的行为；缺失时报cdf_obstacle_missing且不改文档，不再隐式恢复删除的几何。旧场景已存在的URDF目标和显式RobotObject检测器仍兼容，既有参数字段保留，无新增公共API签名或SDK依赖。ProjectScene喷涂测量新增STL对象分支，CPU三角形应用AssetCore单位矩阵及实时对象位姿，沿实际TCP+Z取最近正向命中，原双面法向与带符号入射角规则保留；无命中返回无效，不伪造数值。喷涂导出和图标题更新为A.STL。
CMake配置成功，Release/Debug主程序与受影响领域测试均构建通过；SprayGuidance回归两配置通过（0.48s/2.45s），包含A优先于legacy URDF、删除保存重开、工件全缺失但显式提供旧URDF路径也不导入/不改变文档等检查。测试夹具最初缺名称、后因新增临时文件未清理而失败，均仅修正夹具并最终通过，未放宽业务校验。
两配置均在真实MainWindow默认启动场景中，使用12点调试轨迹完成动态播放、全部12点距离查询、最近距离TXT/折线图及喷涂距离/角度图检查，全部通过；独立读取原A.STL并按实际世界TCP求交，距离误差1.750851e-8m，角度误差1.897615e-5度。Release真实窗口额外执行缺工件场景加载→另存→再打开→再另存，两个保存文件均仅ABB4600，工件为空，无burnner复活。
运行程序C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe（Debug同目录配置下RobotQtViewerdx64.exe）。两配置部署场景与源码SHA一致；证据、事前备份、构建/测试日志、真实窗口报告/图与修改文件SHA清单在C:/b/scene-A-fix-20261020。既有用户ToolSetup、PlaybackWindowProfile等修改保留，冻结Freeform_APF_QP脚本运行库未动。Debug链接仍有既有OMPL缺PDB警告，不影响构建运行。本次未重新规划或重新认证此前交付轨迹。


### 2026-10-20 A.STL部署路径缺失黑屏（修复中）
用户实际运行外层build/Release/bin，存在旧data目录但没有新data/Spray420/A.STL；原MainWindow复现Assimp找不到该文件、ok=false。此前C:/b/rs105-merge没有本地data目录，main的源data回退使验证通过，遗漏了真实部署data目录的分支。改为A.STL与ABB4600-Aburnner.sys.json同目录发布，场景使用A.STL相对路径；保留原data/Spray420/A.STL兼容此前保存的场景。CMake将仅POST_BUILD复制配置改为RobotQtViewerRuntimeConfig依赖目标，每次增量构建也同步配置和场景资源，不创建不完整data根、不覆盖显式DATA_ROOT。将检查两种data布局的真实原窗口及重复构建部署。


### 2026-10-20 A.STL资源部署黑屏（完成修复）
实际用户EXE为外层build/Release/bin/RobotQtViewerrx64.exe；原窗口复现日志明确报Assimp failed to load model: data/Spray420/A.STL，场景初始化ok=false。其本地data根存在但缺新增文件，源data回退不会启用；此前C:/b测试data根不存在，故未覆盖该部署分支。本次将A.STL同SHA放入源码config/projects，与默认场景同目录；默认场景sourcePath恢复为A.STL。原data/Spray420/A.STL保留，另给实际build Release/Debug已有data/Spray420补同文件，兼容此前保存的旧路径；不建立不完整的新data根。
RobotQtViewer/CMakeLists.txt将POST_BUILD复制配置改为RobotQtViewerRuntimeConfig目标依赖，每次构建均复制config及场景内资源，增量无重链接也部署。C:/b与实际用户build均CMake配置成功，Release主目标构建成功，实际用户运行中的EXE未覆盖；仅执行用户build运行配置目标。Debug运行配置部署与重复Release增量部署通过。
验证使用实际用户EXE、实际原MainWindow：默认启动及打开源码新场景均ok=true且robots=1/objects=1/detectors=2；此前data/Spray420/A.STL旧路径场景也恢复。C:/b Debug原窗口打开新场景同样通过。三种场景均真实截图检查，非只判断进程返回值。C:/b Release12点播放、最近距离图、喷涂距离/角度图继续通过。配置及A.STL三处部署SHA一致；完整证据/事前备份在C:/b/scene-A-deploy-fix-20261020。只修改配置部署与资源位置，无运动学/碰撞判据/APF/QP算法变化，无新公共API或依赖。


### 2026-10-28 1.0.6 基础能力升级与 GA/APF/CDF 流程修复（实施中）
目标：保留 A 已有曲面、标定、后处理、GPU 膜厚和 FANUC 无初始轨迹场景，合入 12/1.0.6 的 SDK、整段运动校验、模型环境和构建/测试治理。
范围：cmake/包配置、PrebuiltPackages 的 Common/Core/Platform、ViewerCore/Qt 环境适配、ProjectMotionPlanning 及工作台。现有 main 分支有大量已暂存/未暂存/未跟踪业务变更，不重置；原件和补丁在外层 output/upgrade106_20261028/before。
不变量：检测器绑定当前机器人/实际工件且不隐式复活场景对象；GA 采用 RSW 默认参数，安全控制点经 APF 连接后才可选 CDF；失败/取消不得发布为无碰撞成功；模型环境仅为背景。用户 1.txt 只读。
资源：data/Desktop/environments/factory-detailed 与 data/Desktop/MLRobotUBO.*，需同步公共 shader include 并保持现有业务 shader 兼容。
验证：独立短目录 CMake 配置，Release/Debug 主程序及领域测试；新增接口/环境真实 GL 回归、检测器/距离绘图回归；真实 1.txt 的 GA→APF→可选 CDF、结果验证和日志。实际失败须记录，不能借用上一轮结果。


### 2026-10-28 1.0.6 合入与规划流程修复（完成基础验证）
已在本源码合入 1.0.6 SDK/整段保守运动接口、真实 glTF 厂房环境及构建/测试治理；RSW 业务能力和本轮开始时的 FANUC 无初始轨迹 JSON 保留。检测器按当前机器人/实际工件绑定并识别 selection set；最近距离图支持直接查询关节轨迹。
工作台调用新增领域 GaApfCdfPipeline：实际保存 TCP、RSW 原参数 GA 安全点、APF 点间采样与独立回放、可选 CDF；失败/取消不发布半成品。修复 GA 内部候选数组未初始化崩溃、导入后按钮状态，以及加密 CDF 重新计时时的笛卡尔时间映射错误；新增回归确认真正接受 CDF。无现有 API 重命名或新增第三方依赖。
C:/b/rs106-20261028 的 Release/Debug 最终全目标构建通过；Release 71/71 全量回归通过，Debug 13/13 受影响回归通过。Debug 全量在旧大模型 CDF 查询长用例处主动停止，不称为全量通过；同项 Release 通过。并行测试占用 DLL 导致的一次 Debug 部署失败，在退出测试后完整重建消除。
两配置原生及原 MainWindow 的 3 源点短轨迹均接受 GA/APF/CDF，最终 5 点，距离全部有效且无碰撞；Release 关闭 CDF 的流程也通过。模型环境真实 OpenGL 两配置通过，厂房实际截图已检查。SDK 504 个参考文件与 1.0.6 SHA 一致；修改源码 CRLF 检查通过。
用户 1.txt 原生及真实 MainWindow 均完成实际失败测试：13 个源点，GA 单点 180 秒预算耗尽，碰撞/挡板拒绝，未找到全部安全控制点，因此 APF/CDF 未启动、最终导出禁用；没有完整成功轨迹或连续无碰撞认证。原文件 SHA 不变。GA/APF/CDF 采样门禁不等同于新 SDK 保守运动证书；厂房只作显示背景。
详见 docs/upgrade_1_0_6_integration_20261028.md；备份、修改/资源 SHA 清单和全部证据在外层 output/upgrade106_20261028。运行更新程序请用 C:/b/rs106-20261028/Release/bin/RobotQtViewerrx64.exe，原用户 build 旧窗口未替换。


### 2026-10-28 用户指定 GA 表面球冠与渐进变半径（实施中）
本轮按用户要求修正 GA→APF→CDF 工作台：UI 输入原始喷距（默认100mm），球心 S=P+d*原始TCP+Z，半径 d、法向半角80度、均匀面积随机5000候选；第3/4/5轮仍无安全姿态才开放±30/50/80%原始喷距。原球心与姿态参考不可随结果/重新计时漂移；后续阶段保持实际开放的范围。真实碰撞、安装工具和虚拟挡板检查继续生效，失败不能发布成功。现有大量重叠改动保留，事前副本在外层output/ga-surface-20261028/before。所有语义由ProjectMotionPlanning拥有，Qt只收集喷距。计划涉及领域流程/已有搜索/工作台/回归约10文件，不新增第三方依赖。验证球冠均匀分布/映射/分轮范围/安全父代，以及Release/Debug受影响构建测试和真实1.txt；明确区分搜索未完成与无解。


### 2026-10-28 GA球冠搜索实现与RSW差异定位（已实现，验证边界已记录）
A新增原始喷距UI输入，固定S=P+d*n、80度、5000均匀面积初始池及安全父代进化；3/4/5轮无安全时开放±30/50/80%，原始参考与半径范围传过APF/CDF及重新计时。取消工作台单点180秒限时，默认0无限时，评估上限与取消保留。Release受影响主程序/工具/测试最终构建部署通过，7/7领域回归；真实MainWindow以100mm接受3源点GA/APF/CDF，5点距离均有效无碰撞，导出可用。跨工作台4/4通过；Debug构建通过，6/7回归通过，新增不可能安全壳体夹具的ExternalTrajectoryEvaluation达CTest900秒超时，不称全通过。
1.txt原生100mm实际完成5000池+8轮、末点约679秒21976次仍无安全，验证进程主动停止，不是无解证明，未进入APF/CDF。用户转问RSW差异后，单变量对照定位TCP：A默认yaw约0度，RSW成功保存场景约180度；只改测试副本yaw使原始安全点1/13变7/13，与RSW一致。当前A原生程序复核RSW已知50点轨迹，yaw0报spray_target_error、yaw180全部采样通过。另确认A轴转开关默认false而Agent显式true，以及安全点重搜、进化顺序、后续喷距范围等不同。RSW默认JSON也约0度，不能冒认历史成功场景为默认配置。正式场景与1.txt未改。详见docs/ga_surface_search_20261028.md与docs/ga_rsw_comparison_20261028.md；证据/备份/对照在外层output/ga-surface-20261028。


### 2026-10-28 按RSW对齐GA并重新验证1.txt（实施中）
用户明确要求正式TCP yaw=180度、开启轴转、已安全点不重搜、保留原固定半径24后代并额外演化变半径后代、GA后固定选定逐点喷距。领域层恢复RSW搜索调度并保留取消单点超时；原始球心/法向参考不可漂移，实际碰撞/工具/虚拟挡板门禁保留。改动约7个源码/配置文件，无新依赖；源码与场景备份在外层output/ga-rsw-align-20261028/before。验证固定距离传递/重新计时、已安全点跳过与双种群完整轮次，Release/Debug受影响构建与实际100mm、原始1.txt的原生及UI流程；失败不得当作完成轨迹。


### 2026-10-28 RSW GA 对齐与原始1.txt验收完成
正式 FANUC 场景仅工具TCP yaw改为180度；GA启用喷射轴旋转，已安全控制点保留有效原解并跳过5000池重搜。恢复RSW每轮24个固定半径后代及随机序列，第三/四/五轮无安全时独立追加变半径后代，不消耗固定半径名额；单点0无限时、取消与131072评估上限保留。GA最终按实际FK锁定每个原始控制点喷距，APF/CDF/重新计时/独立回放沿用；修复原始球心检查误用选定喷距的缺陷。领域结果新增gaSelectedSprayDistances（米），无API重命名或新增第三方依赖。
原始桌面1.txt在100mm、真实挂载工具及虚拟挡板开启下，原生与真实MainWindow均完成GA→APF→CDF：13源点→2398 APF点→4795 CDF点，CDF实际接受、非回退。原生最终独立回放和4795点距离/工艺核查通过，最小节点距离1.347281mm，最大瞄准误差0.000995mm、喷距误差0.000988mm；13点喷距均锁定100mm。本输入未触发扩半径，扩半径完整轮次由实际模型回归验证。UI最终导出可用、最近距离图4795/4795有效、0碰撞、0无效，最小节点距离1.377186mm；窗口和图已实际检查，厂房显示正常。
Release/Debug受影响主程序、领域库和工具完整构建通过。最终Release领域7/7及跨工作台4/4通过；Debug六项CTest通过，综合测试首轮900秒超时位于不可能间隙夹具，夹具收紧为恰好216次完整进化预算后直接运行完整退出0，生产预算与安全判据未变。一次测试程序部署因运行窗口占用lib_coacd.dll失败，窗口退出后重建成功。保留初始失败记录，不覆盖用户运行窗口。
证据/备份/本轮差异/哈希清单在外层output/ga-rsw-align-20261028；详情docs/ga_rsw_alignment_20261028.md。运行程序C:/b/rs106-20261028/Release/bin/RobotQtViewerrx64.exe。源场景与Release/Debug部署SHA一致，1.txt SHA仍为23d5a3e1dbcb5d43b0e5ec8cade0588ef9a964973983932c4eac907b79cdae45。结论为实际模型采样碰撞/挡板/工艺门禁与独立回放通过，不是保守连续运动证书；未宣称连续加速度或控制器动力学认证。既有工作区改动保留，RSW与原始1.txt未改。


### 2026-10-28 用户VS2019旧build链接错误修复
用户实际打开外层build/RobotSimulator2026.sln，Release/x64报LNK1120。直接读取其RobotQtViewer.log定位LNK2019/LNK2001：validateProjectSceneEnvironment缺失。源码有实现且CMake已列入；dumpbin证实旧ProjectSceneEnvironment.obj（38291字节，2026-10-28 11:09）没有该符号，新构建对象有。合入源文件保留2026-10-26时间戳，旧build增量判断错误而复用旧对象。此前验收C:/b/rs106-20261028新构建未覆盖用户旧缓存。
已在用户原build定向Rebuild RobotViewerCore（Release/x64，不清理整个解决方案），随后cmake --build build --config Release --target RobotQtViewer --parallel 4完整构建通过；未改业务源码、CMake或调试配置。直接运行实际build/Release/bin/RobotQtViewerrx64.exe，从VS默认项目工作目录启动，加载部署场景，ok=true，保存真实视口截图并正常退出0。用户当前Release/x64链接故障已消除；未声称重建其旧build Debug配置。
证据在外层output/vs2019-link-fix-20261028：original-link-error.log、旧对象备份、core/viewer构建日志、vs-build-startup.log/png、diagnosis.json。若后续再次覆盖保留旧时间戳的源码，需对受影响项目“重新生成”以更新目标文件；本次无需重新写函数或取消环境功能。


### 2026-10-28 GA/APF 加速与均匀约100点输出（验收完成）
按用户要求把 CDF/QP UI 和设置默认值改为关闭，保留手动勾选；用户随后明确 100 点仅为软目标，领域层不因超过目标点数报失败。将轨迹支持节点与0.001rad安全采样解耦，按原始区间均匀分配，必要时保留额外工艺细分并均衡区间；关闭流程中的盲目二次加密。GA 5000池/8轮/固定与变半径后代、实际工具/实体/挡板/奇异性/喷距/时间门禁均保留。
加速包括经实际模型验证的 POE FK/解析雅可比、CPU逆解回退与精确安全检查的独立场景线程池、精确距离/状态缓存、重复初值IK尝试复用、CPU挡板BVH与光线包含筛选，以及保留完整网格归一化的稀疏GPU评分和较大有界kernel批次。不同阶段仍新建场景独立回放，不跨阶段复用安全结论。领域新增可选preferredOutputWaypoints/worldJacobian，无公共API重命名和新第三方依赖；所有受影响依赖已重编译。
原始1.txt、100mm、正式FANUC挂载及虚拟挡板开启：最终原生24.121秒，用户实际外层build原MainWindow按钮24.235秒；13源点输出100点，12区间分成8或9子段（新增7或8点）。100点距离有效/0碰撞/0无效，最小节点距离0.233444mm；工艺和新的独立回放通过。最终并行/稀疏GPU结果与本轮第一版串行100点结果最大关节差0。显式开启CDF原生95.913秒，CDF实际接受、最终仍100点并通过独立回放；此前完整含CDF基线1134.35秒。约24秒接近20秒目标，不承诺固定20秒，不混淆关闭QP与开启QP的功能差异。
用户实际build Release主程序/原生工具及C:/b独立Release工具/领域测试、Debug主程序/工具均构建通过；Release领域7/7与实际build跨工作台4/4最终通过。Release/Debug GPU数值、解析雅可比验证及512组BVH/逐网格对照通过。Debug原MainWindow短轨迹3→100点、默认QP关闭、距离图及导出通过（152.487秒，测速使用Release）。未重复此前长达11分钟的Debug综合用例，不声称Debug全量通过。
首次新测试编译误用TransformDesc接口后已修正；外层build跨工作台初次因测试EXE未构建为Not Run，补建后发现旧用例假设QP默认开启，已改成默认关闭断言+显式选择QP，最终4/4通过，保留初始记录。源C++保持CRLF；大量既有改动保留，RSW/厂房data/正式场景及桌面1.txt未改，输入SHA仍23d5a3e1dbcb5d43b0e5ec8cade0588ef9a964973983932c4eac907b79cdae45。
本轮为细采样门禁和独立回放通过，不是保守连续运动证书。详见docs/ga_apf_speed_20261028.md；备份、限定本轮差异/哈希、构建/回归、分阶段计时、完整轨迹、真实UI与图证据在外层output/ga-speed-20261028。运行更新后的外层build/Release/bin/RobotQtViewerrx64.exe，VS2019使用Release/x64。

### 2026-10-28 用户 VS2019 MSB8066 配置并发修复
用户原外层 build 的 Release CollisionDetectorConfig.log 证实：多个工作台项目同时执行 --check-stamp-file，因源码/CMake 与 cmake.verify_globs 时间戳变化触发全局重新配置。并发配置争用 pinocchio 的 build/cmake/tmp-cxx-standard.cpp，并在 install_headers 的 REMOVE_RECURSE/复制之间发生竞争，导致 write_file/configure_file 失败，汇总为 MSB8066；错误指向的 CollisionDetectorConfig/CMakeLists.txt 不是语法错误根因。
在顶层 CMakeLists.txt 的最低版本声明之后、依赖准备与 project() 之前加每个 binary tree 独立的 file(LOCK ... GUARD PROCESS TIMEOUT 180)，串行保护本工程的配置期写入。相对本轮备份仅新增此锁块，保持 CRLF、原有用户改动、自动重新生成、Debug/Release 分离；无业务代码/API/依赖变化。
用户实际 build 重新配置、Release CollisionDetectorConfig 与 RobotQtViewer 构建及重复增量构建退出0；Debug CollisionDetectorConfig 构建退出0。刻意并发启动两次真实 CMake 配置，第二次在第一次活动期间没有进入 project() 输出，176次100ms观察均等待，两进程最终退出0。Release 实际主程序从 VS 默认项目工作目录启动，场景/厂房/机器人加载 ok=true，截图已检查，正常退出0。未重建 Debug 全部主程序或重跑轨迹性能测试，本轮修复仅涉及配置生成并发。
证据、事前 CMakeLists/cache/错误日志、两次并发日志、构建日志及启动截图在外层 output/vs2019-msb8066-20261028，摘要 diagnosis.json。VS 已打开旧工程时应接受重新加载修改后的项目，或重新打开外层 build/RobotSimulator2026.sln，再用 Release/x64 启动 RobotQtViewer。

### 2026-10-28 曲面规划新增功能合入（验收完成）
已合入算法d241ed2与工作台cb97bf1的上下过渡点姿态/偏移方向解耦、法向开关、独立喷距及跟随模式、QSettings保存恢复，统一算法层位置与逐点spray_distance。选择性吸收挡板配准、孔口边缘与诊断行为，保留本地自交/无效输入检查和GPU/BVH，维护compat源与材料化实现一致。9个源/配置文件CRLF，无新依赖或API重命名。
用户原build的Release/Debug主程序、曲面模块和依赖均构建通过；同5项曲面/真实控制器UI/跨工作台回归在两配置均通过。UI用临时INI隔离验证100mm跟随、80.5/135mm自定义、关闭保留、取消、恢复跟随及重建后120mm全局喷距。专用C:/b构建VirtualBaffleGeometry通过，含512组BVH/完整网格对照。两配置主程序实际加载FANUC/厂房、视口ok=true、退出0并检查截图。
首次用户build因BuildExternalTrajectoryTests=OFF缺少VirtualBaffleTests目标报MSB1009；保留日志，改用已开启的专用构建验收挡板，用户选项未改。最终仅既有C4819和Debug OMPL缺PDB的LNK4099警告。未重复GA/APF/CDF完整性能测试；原1.txt哈希未变。详情docs/freeform_update_20261028.md，证据/备份在外层output/freeform-update-20261028。


### 2026-10-29 2400点分段/整条GA/APF修复（V6复测中）
基线200段143通过；整条2332/2400原姿态有保留IK，67.422秒在初始IK终止。UI默认±360度与物理限位交集补齐多数解；剩余12原姿态未保留合法IK，显式GA允许搜索姿态后补齐，普通IK/验证接口保持缺层关闭。第136至147共12个相关分段和整条的初始构型由完整起始前缀Top-1确定，报告明确范围，不能伪造完整原始Top-1。
APF已完成几何检查但仅时序不足的精确路径，先统一仿射重定时保持原表面站位，再新场景独立回放；未接受路径不直接发布。原始6号段复现通过。增加验证过球形腕的保守不可达筛选、实际POE FK残差、轻量单点IK输入、仅运动学批量排名及点级安全缓存失效范围。5000池/8代/固定和变半径后代/真实限位/工具/实体/挡板/工艺/奇异性门禁保留，无新依赖及CMake更改。ExternalTrajectoryEvaluationResult新增retimingCandidate，需要消费者重编译。
V3全200段199通过，198段多圈编码器连接遗漏由V4修复，66.805秒、81节点、APF重定时及独立回放通过；混合版本增量结果200/200单独保存。V3整条3934.338秒在GA未通过，未进入APF；第63个源点找到49个安全可连接姿态，却被普通候选质量排名全部淘汰。V6独立保留最多32个连接候选，并先尝试局部及实际多圈连接再进行全轨迹姿态场搜索。前72点对照V5在GA失败、V6 GA通过（424.579秒）；额外前缀APF未完成即停止自建测试进程，为用户要求的整条和200段释放资源，不计为完整轨迹通过。用户GUI未终止。
V6 Release/Debug主程序及4项相关CTest通过，源8文件CRLF，源码/静态库/原生测试程序SHA记录v6_build_manifest.json。此前版本双配置GPU几何、FK/雅可比、8192真实FK保守可达性及启动截图验收保持有效，相关实现未再修改。部分测试构建误用了不存在的target名，改用实际target后成功，原失败日志保留。
当前V6统一200段与整条仍在并行，不能提前宣称整条或V6全量通过；20秒目标尚未普遍达到，最终需报告负载条件与隔离测速。此前V3/V4完整输出在output/freeform200_optimized_final_20261028；V6输出在output/freeform200_optimized_v6_20261028，整条在output/freeform_fix_20261028/trials/whole_v6。详细根因和实施说明见docs/freeform_batch_optimization_20261028.md。


2026-10-29 原始IK缺层范围核对更正：逐个读取完整报告后确认，12个原始缺解点对应第136至147共12段，并非先前概要中所写的6段。偶数段136/138/140/142/144/146首缺局部第10点，完整前缀9点；奇数段137/139/141/143/145/147首缺局部第3点，完整前缀2点。整条首缺原始第1630点，完整起始前缀1629点。数值多初值枚举不是全局不可行性证明；证据output/freeform_fix_20261028/original_ik_gap_scope.json。逐段CSV/report原始数据无误，最终说明按12段报告。


2026-10-29 V6/V7追加验收：V6全200段199通过，仅169在APF固定喷距曲面映射的单初值IK失败后终止。V7保留原快速投影，仅失败时使用相邻端点引导的多初值与约束内姿态插值，喷距继续固定为GA逐点最终值，所有实际FK/限位/工艺/碰撞/挡板及新场景回放门禁保留。169复测49.756秒，GA阶段完整输出与V6逐项一致，APF经投影补救及精确路径重定时，最终独立回放和71点导出审计通过。失败原始证据保存在trials/segment169_v6_failed，回归对照segment169_projection_regression.json。
当前合并200/200有接受轨迹：199段V6、169段V7，版本差异在revisions.json与CSV逐段标明；不是V7统一全量重跑。负载下平均33.588秒、中位32.164秒、P95 49.756秒、最大169.020秒，13段低于20秒，未普遍达到20秒。V7 Release/Debug主程序和各4项相关CTest通过，生产源及程序SHA见v7_build_manifest.json。最后V7整条测试已启动，上一V6整条也保留运行作为对照，二者均尚未接受，不能提前宣称整条通过。全部关闭CDF。


2026-10-29 V8局部回退复测中：V6整条4685.938秒结束，零基1761点131072候选预算内未找到连接前缀的安全姿态；GA未通过、APF未启动。V7 GA代码相同，其整条重复任务在1133.885秒主动停止以释放资源，未动用户GUI。V8在当前点全量连接重搜前，最多回退24个原始点，用有界安全beam修复前缀局部姿态，独立保留整条连接根，保持已授权半径、固定锚点、锁定起点及实际编码器/速度/APF独立回放门禁。只改ExternalTrajectoryEvaluation.cpp，CRLF，无依赖或公共API变更。Release/Debug主程序构建和各4项相关CTest均通过；V8整条及代表段仍在复测，不能宣称整条通过。证据v8_build_manifest.json、v8_validation.json和trials/whole_v8。


2026-10-29 V9评分准备并行化验证中：在V8局部回退基础上，仅将VirtualBaffleEnvironment的GPU任务准备拆成最多4个独立CPU缓冲，合并顺序与原pose/body次序一致，CUDA评分、网格归一化及实际物理安全门禁未改；小于64姿态仍串行，资源/异常失败沿用CPU回退。新增VirtualBaffleTests实际512姿态的并行/串行/CPU评分对照。无新依赖/API/CMake变化，C++保持CRLF。正在做Release/Debug构建及回归、7个代表段的V8/V9轨迹逐项比较。整条仍由V8逻辑运行，不以分段拼接冒充整条通过。


2026-10-29 V9统一200段验收完成：200/200均接受GA+APF，CDF关闭。所有导出节点/时间/距离/挡板/工艺与输入哈希审计通过，单一exe SHA4cc1afe166477c8c4b306cae7c8a3d64287ada55367ad55f52c7387371e1b070。并行负载下平均32.243秒、中位30.903秒、P95 50.453秒、最大166.810秒，15/200低于20秒；没有普遍达到20秒。7个代表段V8/V9的GA控制点及最终关节/时间逐项一致；Release/Debug主程序和各5项相关回归通过，含512实际GPU并行/串行/CPU评分对照。交付文件夹200段CSV/ZIP已更新为统一V9。整条V8在1761连接处仍未接通、尚在搜索，不能宣称整条通过；后续修复和隔离测速仍待完成。


2026-10-29 V10长前缀回退验证中：V8整条4460.737秒终止，仍在零基1761点131072预算内无可连接安全姿态，APF未启动，未产生接受整条轨迹。V8的局部回退24点/4096总IK预算可能过早截断不同腕/肘分支；V10沿已有安全候选beam可追溯整个有限前缀，已有前驱可延伸时不重复IK，新增IK总预算16384、单父代最多256提议。保持实际限位、锁定起点、授权半径、图跳变/速度、碰撞/挡板/工艺和最终APF独立回放门禁。原始轨迹Top-1整条V10已另行启动。并行WholePoolProbe仅复用200段GA控制点诊断连接机制，输入名义半径为复用点实测半径，不能冒称原始Top-1整条流水线或最终接受输出；首轮探针误用fixed关闭GA，二轮原名义半径与变距点不符，均保留失败证据，三轮修正后继续诊断。V10构建/回归仍在运行。


2026-10-29 V11统一最终验收已启动（未完成）：修正反向safe候选扫描在首个断点终止，改为从下一安全连通分支恢复扫描、传播前驱，最后仍必须整条图连接及APF/回放通过。V10长前缀回退和V9确定顺序GPU任务准备保留。Release/Debug主程序及各5项CTest通过。V10受影响5段+6代表段共11/11通过；其正式整条约1200秒时为集中V11验收主动停止，未称为通过。完整复用GA点池的两个诊断主动停止；窗口1729..1776诊断几何图连接成立，仅源时间需重定时，不能当作原始Top-1整条GA/APF通过。辅助诊断的输入姿态和名义半径为复用GA实测值，明确区别正式输入。
当前唯一正式长测试为V11整条（trials/whole_v11）和双进程统一200段（output/freeform200_optimized_v11_20261029）。V11整条仍由桌面新版原始输入及完整前缀Top-1初始化，没有导入200段结果或拼接轨迹。源码/程序SHA见v11_build_manifest.json。交付文件夹当前200段为已完成的统一V9结果，待V11完成后更新。原用户GUI PID34592保持打开；用户旧build主程序尚未最终部署。


2026-10-29 V12临时缓存生命周期优化（验证中）：实际V11整条到原始1283点附近，private约1.6GB、working set约0.7GB，系统可用物理内存约1.1GB、压缩内存约1GB，CPU阶段明显变慢。定位到GA长期持有已淘汰搜索根的pointCertificates/pointPenetrations/pointBaffleViolations/gaDistances，且definitelyUnreachable在gpuPostures消费前返回，使不可达预取条目跨全部原始点累积。V12每个完成的searchPoint在完整5000池/8代及transport结束后，仅保留实际候选池相关精确证书，丢弃临时拒绝/淘汰根缓存，并释放该点未使用的GPU预取。以后重提被丢弃根重新计算；随机序列、候选输入、半径轮次、真实碰撞/挡板/工艺/奇异性、图与APF门禁不变，无API/依赖/CMake变化。
V11整条和统一200段继续作为对照保留运行；V12整条从原始输入+原始Top-1重新启动，未复用分段结果。V12两配置构建/回归和精确轨迹对照尚未全部完成，不能宣称缓存清理已验证等价或完整整条通过。源/程序SHA在v12_build_manifest.json。


2026-10-29 V11统一200段完成，V12精确等价验证完成：V11全部200/200接受GA/APF，CDF关闭，输入SHA与导出审计通过；平均32.738秒、中位30.573秒、P95 51.486秒、最大161.289秒，12/200低于20秒。双分段+V11整条并行，后期叠加V12构建和整条，非隔离测速，不普遍达20秒。V12缓存清理Release/Debug主程序及各5项CTest通过；11个代表/回退相关段全部接受，GA控制点和最终关节/时间与V11逐项一致，节点导出审计通过，证据v11_v12_representative_equivalence.json。整条V11/V12仍在运行，暂不能宣称整条通过；截至增量对照177原始点完整进化报告未发现差异。实际进程采样V12处理大量点后private约0.7GB，V11约2.3GB，进度不同不当作严格速度比。


2026-10-29 200段耗时分解追加：统一V11并行负载下GA均值22.512743秒、中位22.794154秒；APF均值4.973130秒、中位2.678520秒；独立复核均值2.667363秒。APF尾部最大126.833820秒（104段），不能仅优化GA后承诺所有段20秒。完整原始点搜索503次，末次累计profile汇总IK685.497秒、CPU safety609.648秒、merit1145.353秒；这些只覆盖带逐点搜索报告的累计区段，不等于整个GA阶段精确不重叠分解。证据output/freeform_fix_20261028/v11_cost_breakdown.json。V11/V12正式整条仍运行，不以分段输出替代。


2026-10-29 V13 GA内部全局图探测缓存复用：发现returnCandidates默认false时graph.nodeAdmissible对全池重新执行stateValid，长轨迹多次图探测重复昂贵实体/挡板/奇异性计算。GA块局部gaGraph改为safeAt，即同一不可变请求场景下精确关节与源点的既有证书，半径变化时仍按原逻辑失效；几何图、绕圈搜索使用其副本。最终GA graph未改，APF和新场景全路径回放未改，5000池/8代/半径及安全阈值不变。仅ExternalTrajectoryEvaluation.cpp，CRLF，无API/依赖/CMake变化。Release/Debug主程序及各5项CTest通过；1/6/137/169/198/104六段完整接受，GA控制点和最终关节/时间与统一V11逐项一致，导出审计通过，证据v11_v13_representative_equivalence.json。V11/V12正式原始输入整条继续对照；V11已越过反向分支扫描进入前缀连接补修，尚未完整通过GA/APF。V13当前无完整整条验收结果，不声称已达成20秒。


2026-10-29 V14双向耦合GA补充（验证中）：V11/V12都已越过原1761瓶颈，但到零基1809，V12已超过10万候选仍无能连接前缀的安全姿态；原安全beam在668次IK试验后结束。新增仅在beam无法延伸时，对最多3个前驱站位做完整5000池/8代GA，连接目标为后继safe beam；这些新增可选补救最多4批额外rescue，正常当前点完整预算不减。保留前驱此前已授权半径区间，只有完整请求代数完成后才允许局部无解回退，任何中断保持致命门禁。最终GA图只读本次刚独立检查的候选标记，避免全池重复物理查询；APF及新场景独立路径复核保持原门禁。V11重复对照在同1809卡点主动停止，保留V12完整原始输入对照，未动用户GUI。V14仅修改ExternalTrajectoryEvaluation.cpp，CRLF，无API/依赖/CMake变更。
正式长测试外部harness增加GA候选池和安全控制点检查点：只改输出目录内测试包装，按当前生产pipeline复制，GA阶段returnCandidates用于导出，APF前恢复false；原始输入全IK/Top1/GA/APF未跳过。第1段包装与标准V14等价验证正在运行。APF专用检查点复现harness已用第6段通过且最终关节/时间与V11逐项一致，但它不含GA耗时、不得作为原始整条新运行。V14两配置构建、代表段及后续整条尚未完成，不能声称20秒或整条通过。


2026-10-29 V14验收进度：Release/Debug主程序及各5项CTest通过；1/6/137/169/198/104六段接受并导出审计通过，其中198因新增前驱GA选择了不同安全构型，最终路径也不同，其余5段GA控制点与最终关节/时间均与V11逐项相同。测试包装的第1段与标准V14 GA/最终关节时间逐项一致。正式V14整条（trials/whole_v14，session70509）和单worker统一200段（freeform200_optimized_v14_20261029，session53309）已从原始输入重新运行，未导入分段优化结果作为整条种子。两项都尚未完成，交付文件夹暂保留已完成V11的200段。
V12对照后来在1809点找到1个可连接安全姿态，共111576次评估，595.468秒；因此此前停滞不是无解证明，当前点搜索本身可以成功但很慢。V14前驱耦合搜索旨在减少这种长时间孤立搜索，同时保留原完整当前点回退，不以未找到临时候选代替最终安全门禁。


2026-10-29 V15真实编码器连接修复验证中：V12完成前缀修复后仍未接通整条图，进入昂贵整体姿态场搜索。发现搜索的模2pi距离与最终图真实编码器距离不一致。V15连接/beam/去重统一真实角差，优先真实可达父代；初始逐点结束后提前保留实际限位内FK等价且逐一安全检查的多圈根，保留原安全解。5000初始候选/8代、碰撞/挡板/工艺和最终独立回放门禁不放宽。Release/Debug主程序和各5项CTest通过，代表段复测中。V14整条主动停止，200段仅完成52/52接受；不得称完整200段或整条通过。V11完整200/200结果继续保留，普遍20秒与整条验收尚未达成。源码与测试程序SHA见v15_build_manifest.json。


2026-10-29 V16连接见证保留：V15六代表段全部接受且导出审计通过；104段150.558秒，其中APF125.035秒，不能声称普遍20秒。继续审查发现reachablePrefixes保存关节副本，但后续transport/局部评分可能淘汰其实际候选根，导致前缀声称可达而最终池缺少连接根。V16单独保存已到达前缀的完整候选见证，在最终图前逐一safeAt复核并补回缺失根；真实编码器最终图/APF/独立回放仍必须通过。5000/8代及物理/工艺门禁不变，只改同一CPP，无API/依赖/CMake变化。V15长测试在224.358秒主动停止，未接受GA/APF；统一200段仅部分完成，保留中断证据。V12对照6208.032秒主动停止；用户GUI34592未动。V16构建验证中。


2026-10-29 V16验证：Release/Debug主程序和各5项CTest通过；统一200段优先运行的1/6/137/169/198/104六代表段均接受、导出审计通过，GA控制点及最终关节/时间与V15逐项一致。负载墙钟23.486/21.995/29.262/41.410/28.869/145.158秒，未达普遍20秒。新完整原始整条已从全IK/Top1重新运行，当前逆序逐点GA，未完整接受GA/APF；统一200段继续单worker运行。用户原build的Release运动规划静态库已重建V16（exit0），Debug正在构建；用户旧GUI未关闭、主程序未覆盖。SHA及状态见v16_build_manifest.json。


2026-10-29 V16附加验证：用户原build的Release/Debug运动规划静态库均重建完成exit0，旧GUI未关闭。独立两配置实际GPU回归各13266挡板评分样例通过，CPU/GPU最大差2.22e-16；5000均匀候选与实际IK/FK检查通过。外部原始缺解诊断对12点用1024种子/240迭代、经原运行时验证的POE纯CPU FK/雅可比重算，仍各0根；球形腕必要可达界未排除这些姿态，因此不能声称已证明原始姿态不可达。该诊断不是GA/APF全流程或正式计时，也未修改生产源码。原始全逆解缺层仍如实记录前缀Top-1范围。V16完整整条与统一200段继续运行，当前25/25段接受、整条逆序GA约1858点；无整条接受结论。


2026-10-29 原始12点缺解补充诊断（非生产修改）：保持实际限位时1024种子/240迭代仍0候选；仅外部探针临时将全部6关节放到+/-360度后，每点可保留512数值根（达到候选上限，存在截断），独立原运行时FK残差通过，但最小观察到的最大关节越界仍0.1069..0.4889度。说明实际限位是重要缺解线索，不是APF单点碰撞问题；不能把已截断数值枚举当解析无解证明。生产与正式V16验收未放宽限位、未导入这些越界诊断根。明细original_ik_limit_diagnostic_summary.json和trials/original_gap_limits_diagnostic。当前71/71段接受、整条逆序GA约946点，仍未完整接受。


2026-10-29 V17搜索中同步恢复连接见证（构建中）：V16真实整条在1223/1224/1256/1258多次回溯至约255..559，各消耗16384个IK提议；虽保存见证但仅最终图前补回，当前搜索仍看不到已被淘汰的旧根与完整历史可达集合，可能重复全GA，甚至提前耗尽当前点预算。V17在beam访问站位和前向扩展时恢复独立安全根，并把历史已到达根并入可达集合；旧半径范围仅由可选前驱探针扩展，仍逐一safeAt检查，实际图/APF/新场景复核不放宽。5000/8代、锁定起点、物理限位及CDF关闭不变。只改同一CPP，无API/依赖/CMake变化。V16完整整条与200段继续作为对照运行，未中断；当前约129段接受、整条连接补修约1258，尚无任何版本整条接受结果。V16源码备份before_v17，V17构建会另冻结SHA和程序，不冒充已验收。


2026-10-29 V17冻结验收启动：Release/Debug主程序与各5项CTest通过；1/6/137/169/198/104六代表段全接受并导出审计通过，GA控制点与最终关节/时间均逐项等于V16。负载墙钟28.771/26.071/36.041/47.713/35.832/162.437秒，不能当隔离测速或声称20秒达成。源/程序SHA见v17_build_manifest.json。V17正式整条从原始2400点与原始Top-1条件重新启动（session55798，trials/whole_v17），未导入分段优化结果；统一V17完整200段已用2进程重新测试（session67520，freeform200_optimized_v17_20261029），不混用V16结果。V16单worker200段和整条仍作为对照，约154段已接受，整条约1761连接补修；尚无任何整条通过。当前交付文件夹继续保留已完成V11完整200段。V17完成后再做无其他测试任务的代表段串行测速，使用run_isolated_v17.py与finalize_v17.py。


2026-10-29 V18局部性能实验：V17第104段完整接受，但APF约159.922秒，query统计motion约150.668秒。当前CdfQueryBatch仅缓存有效运动，APF重复测试完全相同的碰撞边仍重做查询。实验增加同一不可变私有场景、完整精确键下的state_in_collision/virtual_baffle_collision/motion_in_collision拒绝缓存，pathValid命中拒绝必须返回false；取消、运行时/查询失败、不确定判定不缓存，最终独立回放不变。增加真实连续关节夹具回归，验证重复碰撞复用且取消优先。尚在Release/Debug构建验证，不宣称速度提升；V16/V17冻结长测试继续，未重启或混用数据。若无实测收益会回退本实验。

2026-10-29 V18拒绝缓存实验已撤回：两配置构建及各5项CTest通过；104段接受，GA控制点及最终关节/时间与V17逐项一致，但APF156.548秒对比原V17159.922秒均处于不同并行负载，不能确立有效性能收益。为控制改动，CdfQueryBatch.cpp及新增测试精确恢复V17备份；当前生产源码仍V17。外部构建目录静态库暂是实验构建，最终部署前必须按恢复源码重建。冻结V16/V17长测试仍不变。

2026-10-29 V17恢复构建与部署：已核对ExternalTrajectoryEvaluation.cpp与V17冻结SHA一致、两个V18实验文件与V17备份逐字节一致。用户原build的Release/Debug ProjectMotionPlanning静态库均exit0；独立C:/b/rs106-20261028的Release/Debug RobotQtViewer均按恢复源码重建exit0。仍保持旧用户GUI PID34592，未覆盖其运行中的主EXE。构建记录v17_restored_deployment_builds.json。V17统一分段60/60接受、整条仍运行，无整条接受或普遍20秒结论。

2026-10-29 V16统一200段完成：200/200接受，全部输入SHA和导出节点/时间审计通过，CDF关闭。平均29.552秒、中位27.215秒、P95 46.336秒、最大145.158秒，29段低于20秒；单分段worker与整条/新版本回归并行，非隔离性能比较。交付目录的200段CSV/ZIP/审计已升级为统一V16，V11旧交付备份delivery_before_v16_batch。V17分段约89接受、两个整条仍运行，无整条或普遍20秒结论。

2026-10-29 V16整条GA通过、APF启动：原始2400点完整搜索前缀在6110.4秒接到末点，恢复1090个见证；6728.14秒真实编码器几何图Top3接通。第一GA阶段6647.476秒因原时间需延长未接受；固定GA选中逐点喷距、精确控制点重定时后17.235秒新校验接受。ga_checkpoint已实际写入ga_attempt_1.json(约112MB)、ga_attempt_2.json、stage_1.json(2400点)及report.json，半径2400项。APF已启动，仍无整条最终接受结论。V17分段127/127接受、整条原始逆序搜索继续。

2026-10-29 V19单边内部采样并行实验：整条V16已通过GA，APF原生以0.00025rad扫描13831节点。CdfQueryBatch原先单条边只能用一个worker；新增场景私有validateSampledMotion统一保留原步数、比例、连续关节wrap、预算和碰撞规则，公开串行入口仍逐点校验，单边批量入口以32个原样采样为一组在既有私有场景线程池并行。保守SDK入口不变，不共享可变场景、不降低精度，结果按原顺序取首个拒绝。新增真实连续关节串/并行判定、碰撞比例、绕圈、取消及预算回归。仅规划模块5文件，无公开API/依赖/CMake变化，待构建与轨迹等价验证；V16/V17冻结长测试继续。


2026-10-29 V19补齐验证：Release/Debug主程序构建、各5项既有CTest、真正包含新增采样夹具的ExternalTrajectoryEvaluation与ProjectMotionPlanningCdfQueries两配置均通过。6个代表段104/1/6/137/169/198全流程接受、导出审计通过，GA控制点、最终关节/时间与正式V17逐项一致；104段APF66.187秒对V17159.922秒，后台负载不同，不视为隔离速度比。V17正式200/200全部接受并已整理交付，平均37.788秒、中位35.839秒、P95 54.825秒、最大199.578秒、1段低于20秒，为双worker与整条并行负载。整条V16全部APF局部修复区间通过，最终全路径碰撞复核仍运行；V17仍GA，V19检查点APF复现另行计时，不冒充新全流程。当前源码另有V20共享CUDA主上下文两文件实验，GPU13266评分及多对象生命周期回归通过，交替轨迹测速未完成，尚未保留或部署。用户旧GUI34592未动。


2026-10-29 V20共享CUDA主上下文实验已撤回：两文件试验的13266 GPU评分、IK与多对象交替/销毁生命周期回归通过；1/137/169/198四段新旧版本交替运行均接受、导出审计通过、GA和最终轨迹与V17逐项相同，但GA耗时改善6.63%/2.20%/-2.20%/-1.32%，未观察到稳定收益。已按before_v20备份逐字节恢复两文件到验证过的V19，实验源码/程序/对照数据保留output中供复查。独立Release/Debug主程序和既有5项回归正在按恢复源码重建，不把V20当最终优化。V19补充两配置新测试此前全部通过。

补充初始化范围审计：测试包装确实将原始分层图Top-1首构型应用到robot.initialJoints并用于初始IK/候选参考，但独立测试没有设置request.startJoints的硬边界锁定。200段中88段优化首控制点与原始Top-1相同，112段由GA改动首控制点；这与优化不安全姿态的流程一致，不能写成200段首控制点均硬锁定Top-1。模块的startJoints边界锁定功能仍保留，只在调用者显式设置时生效。验收覆盖优化后轨迹，不包含从原始Top-1停靠构型到优化后首点的额外接近运动。明细v17_top1_first_control_scope.json。


2026-10-29 V16完整原始整条最终通过：wall10492.5319096秒（约2小时54分53秒），13864点，GA两尝试6647.476/17.235秒，APF几何及碰撞复核3337.398秒后需重定时，新场景独立validate309.962秒通过，最后导出节点/时间/输入SHA审计通过。audit_publish_whole_run.py已把完整TCP/关节/报告/审计发布到交付目录，不是200段拼接。整条GA检查点和V19APF复现范围明确分开。正式V17完整200段和V16完整200段的全部stage_1及最终关节/时间均逐项相同（200/200）。V17新原始整条仍GA，V19整条APF-only复现仍最终碰撞复核，不能冒充它们已完整通过。V19源码两配置主程序/相关5CTest以及新增2CTest全部通过，原build两配置规划静态库均成功，旧GUI34592未关闭。

<!-- freeform-final-verified-v19 -->
2026-10-29 最终验收：统一V17分段200/200接受；V17原始整条10125.820秒、13844点通过GA/APF/独立回放与导出审计。当前V19采样并行六代表段逐项等价，整条GA检查点APF回放逐项等价。六段串行测速及准确范围见外层output/freeform_final_results_20261029/最终验收汇总.md。20秒未普遍达到；Top-1用于初始化而非优化首点硬锁定；原始12缺层由GA补修；CDF关闭。旧用户GUI未关闭。

2026-10-29 原始安全位姿优先与安全降采样（实现中）：实际旧整条1514个原始点至少有一安全IK，其中528点TCP偏移超过1mm、最大70.721mm。迁移是追加候选，但最终图缺少原始位姿代价。新增最终GA图原始安全位姿优先、偏差次优目标，保留所有限位/连接硬约束。规划域新增安全降采样服务和evaluator downsample入口，目标软约束、保留端点/工艺边界、按原精度验证新关节线性连接并复核动态限制；CDF页面新增点数输入默认4000、后台取消与TCP/实际关节导出，原轨迹不变。未更改第三方依赖；新增API而非重命名。当前尚未构建或整条复测，旧V17结果不代表本轮验收。备份和脚本位于外层output/freeform_pose_downsample_20261029。


2026-10-29 V23构建与界面验证：Release六项相关CTest通过；Debug原轮5项通过但ExternalTrajectoryEvaluation栈溢出。独立Windows调试探针定位为nvrtcCompileProgram在Debug规划深调用栈上消耗过多栈（native_debug_probe.log），GpuBaffleSearch.cpp仅将该同步编译调用移到独立线程，源码/编译参数/CUDA上下文所有权不变；随后两配置ExternalTrajectoryEvaluation回归及GPU13266评分对照均通过，最大CPU/GPU差2.22e-16。Debug新版External测试120.35秒、Release10.36秒。此前lambda拆分及中间C4715已修复；不把失败日志当通过。独立两配置主程序和原build两配置MotionPlanningEditor构建成功。真实普通Qt窗口中以合成201点安全静止轨迹输入37，实际TCP/关节导出均37、首尾时间和关节保持（ui_validation.json）；未关闭旧GUI34592。整条新V21仍从原始2400点全IK/Top1/GA/APF重新运行，CDF关闭；与当前生产GA/APF语义一致，后续V22/V23改动为降采样和栈/编译承载，不重启长测试。旧V17降采样探针目标4000实际9246，仍在独立回放，只作为sampler验证，不冒充新修复结果。正式新整条及约4000点验收尚未完成；finish_whole.py在本轮活跃任务中顺序接续诊断、降采样和新场景回放。源码与程序SHA在外层output/freeform_pose_downsample_20261029/v23_manifest.json。


2026-10-29 新整条GA阶段验收：V21完整735次搜索的point_index/connection_repair/attempts/safe_postures与旧V17全部逐项一致，最后prefix witness已接通；最终图原始安全站位1514个。GA完成并经重定时精确重放接受，APF继续运行，尚非整条接受。新GA中原始安全点TCP偏移>1mm从528降至104，均值10.629945降至1.883120mm；1410点TCP偏移<=0.002mm。最大偏移仍70.720662mm，不能宣称全部大偏移已消除。对104点逐个核对保留的原始安全根：保持当前相邻选择不变时，均不能单独恢复原解并满足现有0.5rad单段门限；最大偏移点1423需至少0.726930rad（remaining_original_changes_adjacency.json）。这是相邻固定条件下的约束证据，不是全局不可恢复证明，未静默放宽连接门限。旧完整轨迹的原始站位审计显示APF另使804/805原安全点偏移1.217/1.429mm，故新增外部ControlPoseAudit工具检查最终/降采样轨迹在原始站位的实际关节插值FK，不将GA控制点统计冒充最终导出统计。诊断/对照为外层output/freeform_pose_downsample_20261029/original_pose_comparison.json、incremental_search_equivalence.json，未改搜索预算与门禁。


2026-10-29 V24局部APF搜索扩展（验证中）：V21新整条GA与重定时精确重放通过，但APF在第9处局部区间7985->7990的三层锚点窗口耗尽，整条未接受、未发布新轨迹，finish_whole.py按门禁停止。已保留GA检查点，不把局部检查当整条通过。CdfQpTrajectoryRepair.cpp仅对启用surfaceProject的引导搜索，在原12/40/120节点与60/120/240mm预判窗口之后追加360/1080节点、480/960mm窗口；实体/挡板/限位/固定喷距/工艺/TCP走廊与采样精度不变，失败时不导出。将在两配置构建和现有APF回归后，从新GA检查点接续全APF和独立回放，耗时与原始GA分开记录。此前CDF自定义点数降采样UI已构建与实际导出通过，整条约4000点仍待新整条通过后运行。


2026-10-29 V24构建及接续状态：独立Release/Debug主程序与原build两配置ProjectMotionPlanning库均成功；本轮两配置5项CTest（APF/Periodic/Wrap、GA/APF detector failure gates、ExternalTrajectoryEvaluation）全部通过，Debug490.63秒。初次Release命令猜错额外测试目标名称导致末尾MSB1009，主程序本身已成功；随后使用实际ProjectMotionPlanning-ApfTests等目标重建并CTest通过，未隐藏该失败记录。真实局部400控制点从新GA检查点接续，APF局部区间均已接通，仍在最终运动复核；完整V24检查点接续已启动（原始全逆解/Top1重做，GA不重复，source SHA不变），当前扫描完整运动；无整条新接受结论。CDF功能新程序单独部署build/Release/bin/RobotQtViewer_pose_downsample_v24.exe，原GUI34592不动；源码/程序SHA及配置见v24_manifest.json。continue_v24.py将在整条通过后自动做目标4000降采样、独立回放和控制点位姿审计，publish_verified.py仅允许验收全通过后发布，尚未执行。


2026-10-29 V24局部真实验收：原始第1001..1400共400控制点，对应新GA关节/逐点喷距精确切片，从检查点接续APF最终通过，2902点，857.599713秒；包含实体/挡板/工艺检查、新场景独立validate和导出节点检查。该窗口内此前对应受阻区间已接通，但局部边界和前序修复历史不同，不能把局部通过等同整条或证明追加窗口必然解决全局受阻点。完整原始整条接续仍扫描初始运动。所有局部结果在local_apf_v24，原始输入未改。


2026-10-29 V25降采样实际运动参考修正（验证中）：审查发现非喷涂/UI导出把FK端点直线当原关节线性运动的TCP参考，可能错误拒绝本来安全且完全等价的原曲线。真实六轴URDF新增三节点同一直线关节运动->两节点等价运动回归，在原V24库上稳定仅该项失败（ctest_v25_before.log），证明不是假设。仅SafeDownsamplingStage.inc的非spray参考改为同一时间原verificationTrajectory的关节插值FK；位置/姿态容差保持1mm/0.01rad，实体/挡板/限位/动态及新边采样不放宽。spray分支原始表面与固定逐点喷距完全不变，GA/APF源码不变，正在运行的冻结V24整条无需重启。修复后两配置重建及External回归待完成。备份before_v25。


2026-10-29 V25验证与部署：新增真实六轴曲线回归在修复前只该项失败，修复后Release ExternalTrajectoryEvaluation11.28秒、Debug132.99秒均通过；两配置主程序构建成功。单独部署build/Release/bin/RobotQtViewer_pose_downsample_v25.exe，UI未更改、未关闭用户GUI，SHA与冻结源码见v25_manifest.json。原build两配置规划库正在重建。另真实局部已验收2902点输入目标1000，安全保留2032点，降采样290.829秒、新场景独立回放281.290秒、合计589.812秒，导出节点检查通过（local_downsample_v23）；不是整条目标4000的最终文件。正式完整V24接续仍扫描初始运动，使用冻结V23 sampler的spray分支与V25逐字节逻辑一致；V25修改仅非spray实际运动参考，不重启正式整条。


2026-10-29 V25交付状态核对：原build Release/Debug ProjectMotionPlanning库均已完成exit0；本轮12个源文件与frozen_v25及v25_manifest的SHA逐一相符、全部CRLF，新部署EXE SHA一致。CDF点数输入/软目标/取消/不改原计划/TCP及实际关节双文件导出已完成，实UI37点夹具、真实局部2032点与两配置回归证据具备。完整V24接续仍在最后一档局部APF搜索，无新整条接受文件。一次性finalize_when_verified.py已接续当前运行，只在whole_completion、全部独立回放/导出节点/源SHA/首尾/原点子集/矩阵数量检查均通过后才运行publish_verified.py；不创建定时任务、不向其他会话发送消息。状态在finalization_status.json，若整条不接受则写明未发布，不拿旧结果替代。后续仍应以实际终态为准。用户GUI34592未关闭。


2026-10-29 V26 APF→GA局部反馈（实现与验证中）：V24整条已于13:04结束，APF仍在原始1190→1191控制点之间受阻，未发布新整条，旧“仍运行”记录过时。本轮在ProjectMotionPlanning域增加类型化APF受阻时间区间，按原始控制点时钟映射反馈窗口；从当前点级安全GA关节/姿态出发、围绕不变表面锚点补充完整候选池，仅补搜窗口、冻结其余控制点，最终图排除已失败关节连接的局部邻域并保留原始安全位姿优先。每轮换采样种子，GA重新选择逐点喷距后再次锁定，随后APF及新场景独立回放；不放宽碰撞/挡板/限位/工艺/采样精度，CDF仍可关闭。默认最多8轮反馈（可取消，无单点3分钟限制），失败仍不发布不完整轨迹。修改3个域头与3个CPP、原有真实六轴回归，无新依赖或UI语义变更。Release/Debug构建、真实反馈回归与原始整条复测尚未完成。证据/备份在外层output/apf_ga_feedback_20261029，用户旧GUI34592不关闭。


2026-10-29 V26整条反馈首次试验未接受：复用同一原始全IK/Top1与已核验GA检查点，原始输入/场景哈希保持，原1190→1191失败区间精确映射到零基1187..1192六点；六点均完成5000初始/8代并有安全候选，但最终新构型图未接通，APF未重新启动，465.246秒终止（whole_v26）。关键1189/1190只有1个可用安全姿态；0.04rad失败邻域排除过宽，而且点级安全过早冻结连接补救的半径探索。V26真实六轴新增6项回归与Release基础APF/检测器门禁通过，两配置主程序构建通过，Debug完整回归仍运行，不冒充整条成功。
V27针对该真实反馈继续改进（构建中）：失败对邻域改为1e-4rad，只作搜索排除；5000初始与所有名义固定半径后代保留，额外增加安全父代的小尺度变异，反馈后代不替代原预算；仅已失败边增加中点/四分点实际碰撞搜索探测，用连接候选而非旧点级安全停止半径补救，最终APF细采样与独立回放仍必须通过。局部GA图不通时增加最多两次窗口扩大和新采样序列，冻结窗口外原解，保留已授权半径范围；总APF反馈仍最多8轮。V27无新依赖或额外API变更。整条接续允许复用本轮V26实际全逆解缓存，但每个原始根重新做实际FK/限位校验、Top1图重新计算，且输入/场景哈希一致，缓存枚举时间不计入新运行；完整APF/反馈/最终运动检查均实际执行。代码/检查点/失败证据位于外层output/apf_ga_feedback_20261029。


## 2026-10-29 19:42 接续状态

V27 Release/Debug 主程序及测试构建通过，两配置相关 CTest 均为 6/6（24.96/838.01 秒）；用户原 build 两配置 ProjectMotionPlanning 库也通过。
原 whole_v27 已真实执行两轮反馈：70.516 秒补搜六点、84.237 秒补搜十点，两次全部 5000 初始候选和 8 代。第二轮实际修改七点、其余 2393 个关节解不动，全局喷距上限仍 112.648mm。第二轮完整 APF 的局部区间已修复，最后日志进入 Final full-path collision verification；19:39 查询时进程已不存在，未生成终态报告，不能视为通过，也没有完整最终输出。
whole_v27_resume 于 19:42 从精确的第二轮 GA 检查点接续，恢复两组失败关节对、反馈轮数、搜索序号及窗口访问次数；原始全 IK 缓存逐根重新 FK/限位验证，Top-1 重算、2400 控制点重新做真实碰撞/工艺检查，随后原生完整 APF 和独立回放。CDF 关闭。接续耗时不含历史全 IK、GA 和前两次 APF，不能作为从零总耗时。
测试独立副本仅增加长验证前的 pending_apf_full_validation.json 观察输出，不改任何碰撞查询或结果，正式七个源码及 GUI 未改；该文件 accepted=false，不能当成验收轨迹。测试以隐藏后台进程承载，避免终端会话中断丢失进度。日志为 whole_v27_resume/run.log，完成后 report.json/execution.json。最终整条仍待验收。

2026-10-29 20:47 接续进展：whole_v27_resume 第一遍 APF 的 31 处局部区间与全路径几何碰撞检查均已通过；为满足固定逐点喷距和原始表面对应关系，refineProcess 新增支撑点到 20578，第二遍 APF 仅 2 处区间且均已接通，现正执行第二遍全路径碰撞复核。仍未完成表面/速度验收及独立回放，不能发布。pending_apf_full_validation.json 是未验收观察文件，accepted=false。

2026-10-29 21:08 V27 整条接续验收通过：whole_v27_resume 原始2400点输入、第二轮反馈后的精确GA检查点，完整APF两遍（首遍31区间、表面支撑修复次遍2区间），几何/实体/挡板/固定喷距/原始表面约束通过；精确几何重定时后新场景独立validate 222.329947秒通过，最终20582点逐点导出审计也通过。exit0，wall5119.129536秒（85分19秒），不是从零全流程总耗时；历史全IK枚举、初始GA及早前APF尝试不在该时间内。输入SHA不变，CDF关闭，最小实体节点距离7.456720281478385e-06m；属于0.00025rad采样碰撞通过，不是保守连续运动证明。独立安全降采样已按目标4000启动（whole_v27_downsample，PID21072），仍需新边检查和新场景回放，实际点数遵循安全软目标。当前正式七源码SHA与冻结V27相符。

2026-10-29 21:39 V27 最终交付完成：整条2400输入从第二轮反馈GA检查点接续，20582点完整APF、重定时、独立回放和节点导出审计全部通过；wall5119.130秒（85分19秒），非从零总耗时。目标4000的安全降采样实际10894点，检查31717候选边，降采样953.842秒，新场景0.00025rad独立回放853.280秒，通过；连同导出合计1868.868秒（31分9秒）。首尾关节/时间保持、原点子集、TCP矩阵数/时间、源SHA及交付文件SHA均已核对。验收轨迹和报告在外层output/apf_ga_feedback_20261029/verified_delivery，说明为验收说明.md；新GUI为build/Release/bin/RobotQtViewer_apf_ga_feedback_v27.exe。两配置主程序和CTest各6/6、原build两配置规划库均通过，正式7源码SHA与冻结V27相同。仅仿真采样保证，精简目标未达到4000但遵守现有门禁，不宣称全局最低点数或保守连续证明；生产外部独立回放步长依旧是流程既有值，不把原生CDF日志中的all stages字样误读成所有外部回放均0.00025rad。所有本轮工作已结束，无需继续等待旧进程；早期失败/中断记录保留。

2026-10-30 正常入口复现工作：此前V27整条为检查点接续，当前补齐公共pipeline对原始Cartesian输入的全IK/Top1初始化，私有document设置初始构型，不硬锁首点；优化结果用Core不透明optimizationMetadata保存原表面/逐点喷距/时间映射，UI降采样恢复相同参考并独立回放。新增字段可选、旧项目仍读写；不替换既有工艺元数据。将用直接链接正式pipeline的回归程序从原始2400点启动，禁用所有旧检查点/失败重放和原始IK缓存。碰撞/挡板/工艺门禁保持，CDF关闭。备份外层output/production_repro_20261030/before；尚未构建、不得宣称从零已通过。

2026-10-30 V28阶段验证：正式公共pipeline补齐全IK/Top1初始化及原始表面/选定喷距/时钟映射保存恢复。独立Release/Debug主程序构建、两配置6项CTest分别25.86/824.34秒全部通过；原VS build的两配置主程序构建通过。真实同场景原前两点从零优化、保存重载、目标2实际3点安全降采样、独立回放与导出审计通过，独立运行目录复测也通过。原2400点整条08:12启动，原始全IK/Top1首构型/前缀1629/图代价及1514安全原始站位与历史一致；前77次GA的点序、尝试数、安全候选数和随机种子逐项一致。整条仍在GA，尚未从零验收。原build Release曾因本轮进程15124占用SprayFreeform/CoACD/TBB导致链接或部署失败；仅同目录重命名保存被加载旧DLL并复制独立新文件，测试未中断，随后Release重建exit0。后续启动脚本改用C:/b独立运行目录避免再次占用；旧DLL待测试终止后移入证据目录保存。UI自动化工具返回未获准控制RobotQtViewer，故本轮UI冒烟未完成，不冒充通过。新增Core可选不透明optimizationMetadata字段（schemaVersion1向后兼容），需全量重编相关消费者，无新依赖。

2026-10-30 V28从零整条GA通过：08:12启动的直接生产入口已重新完成原始2400点全IK/Top1及完整GA，735次点搜索的点序/连接补修标记/尝试次数/安全候选数/随机种子与V21逐项一致；最终2400个选中喷距逐double完全相同（最大差0），最终图已连到终点，重定时后2400/2400点级检查通过。日志7107.49秒进入APF，尚非整条运动验收通过。正式源码及原始输入SHA未变，未读旧检查点。比较记录在production_repro_20261030/rolling_ga_equivalence.json及initial_ga_distances_comparison.json。待完成完整APF/必要反馈、独立回放、保存重载降采样与导出审计，再清理保留DLL并最终复核VS构建。
