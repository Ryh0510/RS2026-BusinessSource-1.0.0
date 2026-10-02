# Business Source 1.0.5 合并记录

日期：2026-10-02。

## 范围与结果

- 目标：`C:/Users/14390/Desktop/RS2026-BusinessSource-1.0.0/RS2026-BusinessSource-1.0.0`。
- 来源：`C:/Users/14390/Desktop/RS2026-BusinessSource-1.0.0/RS2026-BusinessSource-1.0.5-win64-vs2019/RS2026-BusinessSource-1.0.5-win64-vs2019`（实际源码位于同名内层目录）。
- 目标原目录名为 1.0.0，但升级前 CMake/SDK 实际为 1.0.3，并有部分 1.0.4 应用更新以及本地运动规划定制。本次以两侧当前文件为依据，用已有 1.0.3/1.0.4 发布文件辅助三方合并。
- 比较覆盖发布包 2,432 个文件：1,968 个相同、363 个同路径不同、101 个来源新增；目标另有 3,493 个独有文件。排除 Git、IDE 缓存、build/out/log，不把构建产物当源码。
- 最终业务源码、配置、构建和上游说明更新 221 个文件：74 个新增、147 个更新；此数字不计本次 snapshot、audit、报告和 CSV。
- PrebuiltPackages 整体升级为 1.0.5，共 1,875 个文件与新版逐个 SHA-256 匹配；原 SDK 中新版不再分发的 506 个文件退出当前 SDK 集合，完整保存在备份中。不能混用新头文件与旧 DLL/lib。
- 未修改原 data、thirdparty、本地依赖目录、用户 ABB4600 工程或输入轨迹。保留现有子仓库与 Git 历史，未创建提交。

## 更新内容

| 范围 | 已合入内容 | 主要位置 |
| --- | --- | --- |
| Camera-on-Hand | Camera 定义、安装和绑定、运行/显隐、预览窗口、光学坐标系及相关编辑流程 | ToolAssetEditorWidget、ToolSetup 各 task controller、CameraPreviewWidget、RobotViewport；新版 Sensor/Simulation SDK |
| 自定义可视化 | 自定义三角网格、彩色透明平面、高度场、网格生命周期/显隐接口和示例 | VisualizationSDK、ProjectScene、RobotViewport、CustomMeshViewerQuickStart |
| 碰撞系统 | 新中立 backend API、Capsule、能力和范围查询、距离/接触/最近点、逻辑组件及运行依赖部署 | 新版 Collision/SimulationRuntime SDK、CollisionConfigWorkbench、CollisionRuntimeResultsView |
| 场景运行时 | 场景拆分为文档投影、附件、相机、环境、交互、碰撞显示和 Stewart 显示系统，视图引用统一运行态 | RobotViewerCore/ProjectScene*、ProjectRuntimeTypes、ProjectRuntimeBuilder |
| 项目与装配 | 事务与原子绑定能力、frame 解析、设备/安装位/对象绑定任务划分 | 新版 SimulationProject/ProjectSimulationSDK、ToolSetup、SceneExplorer、DocumentController |
| 工作台 | contribution factory、生命周期、插件和 profile 管理；避免非活动工作台处理逐帧事件 | Shared/Workbench*、MainWindow、各工作台 Lifecycle/Contribution |
| 界面与渲染 | 新版 PBR/色彩/法线、环境和显示选项、窄面板与高 DPI 布局；中英文词条同步 | RobotViewport、ProjectSceneEnvironment*、RobotQtWidgetUtils、Theme、config/translations |
| 构建与示例 | 项目版本 1.0.5、sdk.lock、CTest/BUILD_TESTING、独立 BuildSdkExamples 和新增回归 | 根 CMake、cmake、各模块 CMake、examples/regression |

以上为来源包已经提供的能力，本次完成集成；并非重新实现 SDK 内部算法。该包没有 Core/Platform 实现源码，SDK 内部仅能比较公开头文件、CMake 元数据及二进制文件。

## 保留的本地功能

- 直接对导入笛卡尔控制点求全逆解；实际模型 FK/TCP 校核、既有机器人关节符号映射、肩/肘/腕分类和 turn 信息。
- 全局 Top-M、固定起点 Top-K、构型选择对比、两页结果及传入 CDF 初始关节轨迹。
- APF 避障、现有平滑与降采样、一轮 QP、CDF 并行查询/缓存、后台进度和过期结果保护。
- 单点应用与动态播放、末端轨迹线、喷涂距离/角度、轨迹球形点开关及 TXT 导出。
- 长轨迹分层图结果表的批量替换和分页卡顿修复。

已对照备份校验 48 个运动规划算法及关键界面文件，内容未改变。MotionPlanning 子仓库算法无修改；工作台子仓库只迁移控制器视口接口并合入新版生命周期。

## 合并时额外完成的兼容适配

1. **独立运动规划视口接口**：新增 `IRobotQtViewerMotionPlanningViewportPort`，由 DocumentContext 注入 MainWindow 创建的 adapter；FK、关节应用、末端轨迹、控制点 overlay 和测量迁入该接口。碰撞配置改用新版 collision port，避免因旧 `viewportServices()` 不再注册而失效。
2. **实际喷嘴 TCP**：保留既有 ABB4600 标定的喷嘴 link、平移和姿态；适配新的场景附件系统。RuntimeRobot 拷贝、移动及容器重分配继续保留喷嘴标定，增加对应回归。
3. **着色器 DLL 部署**：预编译 RenderCore 动态加载着色器资源，但原消费工程未带入其资源 DLL。现在按 Debug/Release 复制匹配的 `RenderCoreShaderResources`，保持其运行资源身份，不把它变成公开链接依赖。
4. **独立构建数据路径**：CTest 使用配置的 SMROBOT_DATA_ROOT；主程序在未指定环境变量、SDK 默认数据目录无效时使用本项目配置的数据目录，显式环境变量和有效部署目录仍优先。
5. **工作台名称**：采用新版拆分后的 Project Assembly / Tool Setup 名称，去除旧局部重命名造成的重复项。
6. **Business Source 构建检查**：缺失 GTest source-only helper 时使用可选标准查找；目前这些业务回归不依赖 GTest。架构检查执行现有业务代码的 include 边界检查，明确跳过包中不存在的私有 Core/Platform 源码检查。
7. **测试接口和样例**：旧碰撞测试 fake 迁到 collision port，生成资产遵循 project:// 及 manifest；通用视觉碰撞测试加入独立 STL 网格，替代本机缺失的 MyTool01.STL 外部样例。异常现在输出具体诊断。

## 验证证据

独立构建目录：`C:/b/rs105-merge`。使用 VS2019 x64、现有 Qt/第三方库；未覆盖外层旧 build 的 EXE/DLL，也未终止用户程序。

| 检查 | 结果 |
| --- | --- |
| CMake configure | 通过，版本 1.0.5 |
| Release 全部已启用目标构建 | 通过 |
| Debug 全部已启用目标构建 | 通过 |
| Release CTest | 31/31 最终通过；30 项见 tests-release-resources.log，最后修正的碰撞工作流见 test-collision-final.log |
| Debug 回归 | 31/31 最终通过；30 项见 tests-debug-final.log，CDF 查询超过首轮 180 秒预算，单独以 900 秒预算复验，195.78 秒退出 0，最大梯度差 0（debug-cdf-query-extended.log / debug-cdf-query-result.json） |
| 主程序实际启动/默认 ABB4600 场景/自动退出 | Release、Debug 均退出 0；无需手动设置数据环境变量 |
| CDF GUI 后台进度、响应、提交与过期结果拒绝 | 专项测试通过 |
| SDK 完整性 | 1,875 个来源文件 SHA-256 一致 |
| 源码文本 | 208 个变更的源码/构建文件 UTF-8、CRLF、无冲突标记；git diff --check 通过 |

真实输入 `C:/Users/14390/Desktop/11111.txt`：

- 749/749 个控制点完成，5,986 个有效候选，单层 4～8 个，0 个被截断层。既有数值多种子搜索不承诺数学意义上的逆解完备枚举。
- 独立实际 FK 校核最大位置误差 **0.000999176 mm**，最大姿态误差 **0.0000541424°**。
- 全局 Top-30、8 个起点各 Top-3（24 条）通过独立累计代价、去重、时间和关节值保留检查。
- 全逆解结果反复应用后保留、播放、两页结果及 CDF 初始解传递、2001 行 TXT 导出检查通过。

本次没有重新做完整长轨迹 QP 耗时基准，不对新版本作提速倍数承诺；未逐项手工穷举所有相机安装、外部设备、生产许可证或插件组合。私有 SDK 源码和 ABI 全源构建检查不在该 Business Source 可验证范围内。

## 文件清单、程序和备份

- 文件级差异及最终处理方式：[upgrade_1_0_5_file_changes.csv](upgrade_1_0_5_file_changes.csv)。含 SHA-256；`preserved_local` 表示有意保留原定制，`old_sdk_backed_up_and_retired` 表示退出当前 SDK、原文件仍在备份。
- 全量文件对照：外层 `build/upgrade-105/final-file-comparison.csv`。
- 原始比较/处理记录：外层 `build/upgrade-105/comparison.json`、`decisions.json`、`applied.json`。
- 新 Release 程序：`C:/b/rs105-merge/Release/bin/RobotQtViewerrx64.exe`。
- Release EXE SHA-256：`34C353815433C6E894654E04536D5E6E4818FEDD31526940EF136BCC71294B1A`。
- 新 Debug 程序：`C:/b/rs105-merge/Debug/bin/RobotQtViewerdx64.exe`。
- 原 740 个业务文件：外层 `build/upgrade-105/backup/source`。
- 完整旧 SDK：外层 `build/upgrade-105/backup/PrebuiltPackages`。
- 所有本轮构建、测试和实际轨迹 CSV：外层 `build/upgrade-105`。

不要只把新 EXE 复制到旧程序目录运行：它需要同一构建目录内配套的 1.0.5 DLL、着色器资源、Qt 组件和 config。若需回退源码，应先对照 CSV 保存后续修改，再成套恢复备份；不要把新旧 SDK 文件混放。
