# Change Log

本文档记录 RS2026 各正式版本中面向使用者和集成方的重要变更。

## [1.0.5] - 2026-09-30

### 新增

- 增加 Camera-on-Hand 完整工作流：项目级 Camera 定义、重复安装、安装位绑定、独立运行/显隐、可拖动缩放预览及光学坐标系。
- `VisualizationSDK` 增加自定义三角网格、彩色/透明平面和规则高度场曲面 API；提供主工程与安装后 SDK 双模式 QuickStart。
- Collision 增加中立 backend 接口、Capsule、能力查询、场景范围、距离/接触/最近点及空间查询，并提供 Base、Robot、Tools、Presentation 逻辑组件。
- `ProjectSimulationSDK` 增加稳定 runtime session 与实体查询；`SimulationProject` 增加事务、原子绑定/解绑和 frame 解析能力。

### 变更

- `SimulationProject` 统一拥有持久项目事实，`SimulationRuntime` 统一拥有执行状态，Viewer 只承担 presentation 和 interaction。
- `SMRobotPlatform` 源码按 Foundation、Simulation、Sensors、Presentation、Facades、Internal 六个 family 整理；target、DLL、include 和 package identity 保持不变。
- RobotQtViewer 改进 PBR 材质、法线、色彩空间、补光、接触阴影、抗锯齿、无限网格深度和环境预设。
- SDK QuickStart 默认进入主 solution；`BuildSdkExamples` 与旧 `BuildExample` 独立，可全局或按 package 控制。
- SDK ABI 基线从 v6 晋级为 v7；正式 SDK profile 相对 v6 新增 786 个导出、删除 0 个。既有兼容 API 和符号继续保留。

### 修复

- 修复 Camera/Tool Setup 面板显隐、窄面板和高 DPI 命令栏、预览关闭后恢复、多 Camera 并行运行和 Frame 编辑提交问题。
- 修复非活动 Workbench 仍处理逐帧事件及碰撞结果过度刷新造成的性能损耗。
- 修复 Collision 私有 FCL/CCD 运行时部署闭包，以及 Visual Studio solution 中内部 target 重名问题。
- 修复许可证验证缓存并发路径；更新开发许可证计算机信息，不改变正式生产许可证部署方式。

### 发布与兼容性

- 工程、SDK、Business Source、Runtime 及 Windows/Ubuntu 发布脚本默认版本统一为 `1.0.5`。
- 项目 `.sys.json` 继续使用 version 3，1.0.4 项目无需迁移；Product Profile 继续使用 v2，Workbench Plugin manifest 继续使用 v1。
- `ProjectRuntimeBuilder` 五个旧 helper、`ProjectSession` 两个可写 flag 引用及 `ModelManager` 两个路径 cache API 从本版本起正式标记弃用并保留兼容。
- 未新增第三方依赖，C++ 标准保持 C++17。详细升级顺序、冲突热点、API 清单和验证证据见 `docs/releases/RS2026-1.0.5.md`。

## [1.0.4] - 2026-08-27

### 新增

- Product Profile v2、Workbench 组合与插件 v1 部署闭包。
- Profile 管理、切换重启、选择文件恢复及插件完整性/兼容性校验。

### 变更

- Viewer 内部术语和生命周期从 Mode 收敛为 Workbench；持久稳定 ID 与 plugin v1 Mode 字段保持兼容。
- 完善中英文运行时切换、主题、Ribbon 和 Workbench footer 操作。

### 发布与兼容性

- 1.0.4 项目文件格式与 1.0.3 兼容；Product Profile v1 需要显式迁移到 v2。
- SDK ABI baseline 继续使用 v6，未新增第三方依赖。完整说明见 `docs/releases/RS2026-1.0.4.md`。

## [1.0.3] - 2026-08-25

### 新增

- 建立 `project://`、`data://`、`smrobot-asset://` 资产协议及项目伴随资产目录，支持项目迁移、校验和事务式另存为。
- 增加场景对象、机器人链接和挂载附件的统一碰撞模型生产流程，以及 CoACD 不可变 revision 和 current selection 管理。
- 增加 Product Profile、Workbench 组合与事务式生命周期，支持运行时中英文切换和结构化操作状态历史。
- Windows 与 Ubuntu 增加 SDK、Business Source、Runtime 发布编排、ABI snapshot、消费者矩阵和归档校验。

### 变更

- RobotIO 统一解析 URDF 相对资源、`package://`、`package.xml` 和显式搜索目录。
- RobotQtViewer 完善全屏、点云导入、文件对话框策略、任务面板与控件视觉层级。
- 优化模型 CPU/GPU 缓存和场景资源释放，减少重复解析与 Geometry 构建。
- Business Source 只发布业务源码和所需 prebuilt，不再内嵌 `data/`；运行数据由部署环境通过 `SMROBOT_DATA_ROOT` 提供。

### 修复

- 修复普通保存、另存为、碰撞模型持久化、旧凸包清理、Windows 瞬时文件占用和长路径 staging 问题。
- 修复工具绑定后源场景对象继续参与碰撞而产生重复碰撞实体的问题。
- 修复 Business Source 独立 Debug/Release Viewer 构建后的 CoACD runtime variant 部署。
- 修复 Verification Developer Kit 兼容版本选择及旧缓存迁移。

### 发布与兼容性

- 项目、SDK、Business Source 和 Runtime 版本统一为 `1.0.3`。
- C++ 标准保持 C++17，未新增第三方依赖。
- SDK ABI 基线升级到 v6；既有公开 API 未发现移除或改名。
- Business Source 必须包含本 Change Log，且禁止包含 `data/`、`AGENTS.md`、Git 元数据、调试符号和内部授权组件。

## [1.0.2]

- 上一个发布基线。更早版本的详细变更未在当前仓库中形成可核验的统一 Change Log，因此不追溯补写。
