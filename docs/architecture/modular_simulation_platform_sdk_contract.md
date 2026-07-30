# 机器人仿真平台模块化 SDK 与软件包架构准则

本文记录下一阶段项目级调整的长期准则。目标不是立即重写现有代码，而是为后续把 `RobotQtViewer`、工作模式、仿真能力和喷涂扩展逐步拆成可安装、可预编译、可二次开发的软件包建立判断标准。

## 1. 背景与目标

当前平台已经具备机器人构建、装配、碰撞检测、时间推进和简单关节调整等基础能力。下一步目标是让平台成为可用于轨迹规划、喷涂工艺、涂层分析、数字孪生和其他仿真任务的二次开发平台。

长期发布形态应支持：

- 平台基础能力优先以 SDK / prebuilt package 形式交付。
- `RobotQtViewer` 作为最终可执行程序和 Qt shell 存在，但不私有化仿真业务语义。
- 各个工作模式以同级软件包存在，可以被选择构建、选择安装、选择预编译消费。
- 第三方开发者可以只拿到指定扩展包源码，例如 `SMRobotSpray` 和 `RobotQtViewer` 壳层源码，其余基础包来自 `PrebuiltPackages`。
- 新业务包可以在不修改平台基础源码的情况下，把模型、路径、热力图、颜色条、切面、分析结果和专用面板接入主 viewer。

## 2. 总体分层

推荐长期分层如下：

```text
RobotQtViewer.exe
    Qt application shell / composition root
    workbench package loader / registry
    top-level menu, dock, status and window lifecycle

Workbench Packages
    SMRobotWorkbenchProjectAssembly
    SMRobotWorkbenchCollisionConfig
    SMRobotWorkbenchRobotRun
    SMRobotWorkbenchMotionPlanning
    SMRobotWorkbenchSprayProcess
    SMRobotWorkbenchPaintingAnalysis
    SMRobotWorkbenchDigitalTwin
    optional third-party packages

Viewer / Extension SDK
    workbench registration API
    document/runtime command API
    scene visualization contribution API
    panel/action/tree/overlay contribution API

Platform SDK Packages
    SMRobotPlatform
    SimulationProject
    SimulationRuntime
    SceneCore / RenderCore / CameraCore
    RobotRenderBridge

Core SDK Packages
    SMRobotCore
    RobotSDK
    RobotCore / RobotRuntime / RobotIO / Collision / Kinematics
```

`RobotQtViewer.exe` 是组合器，不是业务包本体。工作模式包可以提供 Qt 控件和 controller，但持久项目语义、运行时语义、碰撞语义、轨迹语义、喷涂语义和分析结果仍应落在 Core / Platform / 对应业务 SDK 中。

## 3. 软件包层级

长期软件包应以包为一级单位，而不是以 `RobotQtModules` 内部 target 为最终边界。

建议一级包：

- `SMRobotCore`：机器人、运动学、碰撞、基础 headless SDK。
- `SMRobotPlatform`：项目文档、运行时、场景、渲染桥、相机、资产等平台能力。
- `SMRobotApps`：平台 app shell、通用 viewer core、必要的应用适配层。
- `SMRobotWorkbenchProjectAssembly`：项目装配 / 建模配置工作模式。
- `SMRobotWorkbenchCollisionConfig`：碰撞配置、碰撞模型配置、碰撞检测器配置、碰撞结果查看。
- `SMRobotWorkbenchRobotRun`：仿真运行、时间推进、关节/轨迹执行和运行状态查看。
- `SMRobotWorkbenchMotionPlanning`：路径点、轨迹段、规划约束、可达性检查和轨迹预览。
- `SMRobotWorkbenchSprayProcess`：喷涂工艺、喷枪、喷涂区域、工艺参数和喷涂路径。
- `SMRobotWorkbenchPaintingAnalysis`：涂层厚度、热力图、统计结果、颜色条和分析导出。
- `SMRobotWorkbenchDigitalTwin`：真实控制器连接、映射、遥测、安全状态和受控命令入口。
- `SMRobotSpray`：喷涂领域算法与业务 SDK，可被 `SMRobotWorkbenchSprayProcess` / `SMRobotWorkbenchPaintingAnalysis` 消费。

包内可以继续拆成多个组件，例如 core、runtime、viewer、qt widgets、controllers、examples、tests、docs。包外只暴露明确的 public components。

## 4. 源码包与预编译包

每个一级包都应支持两种消费方式：

```text
UsingPrebuilt_<Package>=ON
    从 PrebuiltPackages/<Package> 查找已安装的 config、headers、libs、dlls。
    当前源码树不再构建该包源码。

UsingPrebuilt_<Package>=OFF
    从源码构建该包。
    如果 BuildPackage_<Package>=ON，则参与当前工程构建和 install。
```

长期交付给二次开发者的最小形态可以是：

```text
PrebuiltPackages/
  SMRobotCore/
  SMRobotPlatform/
  SMRobotApps/ 或更小的 viewer SDK 包
  SMRobotWorkbenchProjectAssembly/
  SMRobotWorkbenchCollisionConfig/
  ...

SMRobotApps/RobotQtViewer/ 或专用 app shell 源码
SMRobotSpray/ 或某个业务扩展包源码
```

如果某些 Qt app glue 暂时无法稳定 ABI 化，可以阶段性开放源码，但必须记录原因和收敛条件。目标是让需要开放的 app glue 越来越少。

## 5. RobotQtViewer 的长期职责

`RobotQtViewer` 应保留为平台最终可执行程序和组合根。

它可以负责：

- 创建 Qt application、主窗口、dock、menu、toolbar、status bar。
- 初始化项目 session、viewer context、event hub、document/view registry。
- 加载内置或外部 workbench package。
- 把包注册的 panel、tree projection、action provider、overlay provider 组合到窗口。
- 处理顶层文件对话框、错误提示、配置入口和应用生命周期。
- 适配 viewer shell 持有的 viewport、window、Qt service 到稳定接口。

它不应负责：

- 项目装配、碰撞、轨迹、喷涂、涂层分析、数字孪生等业务语义。
- 持久 project schema、save/load、dirty 规则。
- 运行时状态、碰撞过滤、轨迹执行、喷涂结果计算。
- 各工作模式私有的 panel、tree、context menu、command 实现。
- 第三方扩展必须修改的硬编码分支。

判断标准：新业务包接入主 viewer 时，应优先注册能力，而不是修改 `MainWindow` 的业务 switch。

## 6. 工作模式包的职责

每个工作模式包是同级能力包，不是 `RobotQtViewer` 下的零散文件夹。

工作模式包可以提供：

- `WorkbenchPackage` 描述信息：id、名称、版本、依赖包、默认启用状态。
- `WorkbenchMode` 注册入口：模式 id、显示名称、图标、默认布局和进入/退出策略。
- `TreeProjectionProvider`：把项目/runtime 状态投影成该模式的左侧树。
- `PanelProvider` / `PanelResolver`：根据 selection 和 mode 生成右侧面板。
- `ActionProvider`：提供 menu、toolbar、context menu actions。
- `ViewportOverlayProvider`：贡献轨迹、碰撞、喷涂路径、厚度图、颜色条、切面等显示层。
- `TaskController`：管理当前模式内可取消的任务、preview、apply/cancel。
- `DocumentCommandAdapter`：调用 Core / Platform / 业务 SDK 的命令入口。

工作模式包不应私有化：

- 稳定实体 ID 和项目事实。
- 多个模式共享的 command 业务实现。
- 应该可 headless 测试的运行时行为。
- 其他包也需要消费的可视化基础能力。

## 7. 扩展接口必须覆盖的能力

为了让 `SMRobotSpray` 或第三方包在基础平台预编译后仍能扩展显示和交互，平台 SDK 必须逐步提供以下接口族。

### 7.1 项目与运行时接口

用于读取和修改项目事实：

- 稳定 `EntityRef` / `RuntimeEntityRef`。
- 项目实体增删改查命令。
- 运行时构建、刷新、时间推进和结果查询。
- 轨迹、喷涂任务、涂层结果、外部设备映射等业务实体的扩展槽位。
- 清晰的事件通知和 dirty 规则。

### 7.2 可视化贡献接口

用于在预编译 viewer / platform 基础上添加显示：

- 加载并显示模型、点云、路径、线段、坐标系、mesh、primitive。
- 添加 trajectory preview、spray path、thickness heatmap、section plane、analysis marker。
- 添加颜色条、图例、标尺、统计 overlay。
- 控制显示层级、可见性、拾取、highlight 和生命周期。
- 将可视化对象绑定到项目实体或 runtime 结果，避免 viewer-only 孤儿对象。

### 7.3 Qt 工作台接口

用于把业务 UI 接入主窗口：

- 注册 workbench mode。
- 注册 dock panel / task panel / property panel。
- 注册 tree projection 和 context menu provider。
- 注册 toolbar/menu action。
- 注册 selection policy 和 panel resolver。
- 注册 task preview / apply / cancel 生命周期。

### 7.4 包发现与加载接口

用于选择启用或禁用某个包：

- CMake 配置期：`find_package`、components、`UsingPrebuilt_<Package>`。
- 运行期：内置 registry 或 plugin manifest，声明可用 workbench 和依赖。
- 版本期：包版本、ABI 版本、最低平台版本、兼容性检查。

第一阶段可以采用静态链接注册；当接口稳定后，再评估运行时 DLL plugin 加载。不要在 ABI 和生命周期未稳定前强行做复杂运行时插件系统。

## 8. 预编译包导出准则

每个可发布包都应明确区分：

- Public components：第三方可以链接和包含头文件。
- Compatibility components：为了旧项目或迁移保留，但不鼓励新调用。
- Internal components：只供包内部使用，不安装公共头，不承诺 ABI。
- App components：可执行程序或 app shell，通常不是算法 SDK。

安装形态建议：

```text
<Package>/
  bin/
  lib/
    cmake/
      <Package>/
      <Component>/
  include/
    <PublicComponent>/
  docs/
  examples/
```

新增 public component 前必须回答：

- 谁是外部用户？
- 该接口是否需要跨 DLL 边界？
- public header 是否暴露了内部类、Qt widget、OpenGL handle 或不稳定 schema？
- 是否有最小 consumer example？
- 是否有 ABI / version 策略？
- 是否能通过 `find_package(<Package> CONFIG REQUIRED COMPONENTS ...)` 消费？

## 9. 包依赖方向

推荐依赖方向：

```text
Business Workbench Package
    -> Viewer Extension SDK
    -> SMRobotPlatform
    -> SMRobotCore

RobotQtViewer.exe
    -> Viewer Extension SDK
    -> selected Workbench Packages
    -> SMRobotPlatform / SMRobotCore
```

禁止默认出现：

- `SMRobotCore` 依赖 Qt、OpenGL viewer 或业务工作模式。
- `SMRobotPlatform` 依赖某个具体 app shell。
- `SMRobotSpray` 为了显示结果直接修改 `RobotQtViewer/MainWindow`。
- 某个工作模式直接调用另一个工作模式的内部 widget 或 controller。
- public SDK 头文件包含私有实现路径。

允许的共享应通过稳定接口完成，例如 command facade、view model、entity ref、visualization service、workbench registry。

## 10. Project Assembly 的特殊定位

`Project Assembly` 是默认入口模式，但不应因此成为 viewer shell 的一部分。它应作为第一个被拆出的工作模式包，用来验证包化设计。

它应集中承载当前 `SMRobotApps` 中与项目装配、建模配置、对象加载、绑定、mount、attachment、基础 transform 编辑有关的内容。后续从 `RobotQtModules` 中迁移时，应按功能归属移动到 `SMRobotWorkbenchProjectAssembly` 包，而不是继续扩大 `RobotQtViewer`。

默认启动行为可以是：

```text
RobotQtViewer starts
    -> loads enabled packages
    -> Project Assembly package registers ProjectAssemblyMode
    -> shell selects ProjectAssemblyMode as default if available
    -> if unavailable, shell enters a minimal empty/project browser mode
```

这样平台可以在没有 `Project Assembly` 包时仍能启动，只是缺少该模式能力。

## 11. 喷涂二次开发场景

喷涂平台交付给第三方时，理想状态是：

- 基础机器人、项目、场景、渲染和 Qt shell 来自 prebuilt packages。
- `SMRobotSpray` 或客户业务包以源码开放。
- 客户通过公共接口注册喷涂路径、喷涂任务、厚度分析结果和 UI 面板。
- 客户不需要修改 `SMRobotCore`、`SMRobotPlatform` 或 `RobotQtViewer` 私有代码。

典型扩展示例：

```text
SMRobotWorkbenchSprayProcess package
    registers SprayProcessMode
    adds SprayTaskPanel and SprayPathTreeProjection
    calls SprayCore / SprayPathPlanning services
    contributes spray path overlay to viewport

SMRobotWorkbenchPaintingAnalysis package
    registers PaintingAnalysisMode
    reads thickness result from runtime / analysis SDK
    contributes heatmap mesh, color bar and section plane overlay
    provides result statistics panel and export actions
```

如果某个喷涂需求必须改动基础包，优先判断它是不是通用平台接口缺口。如果是，应把接口补到平台 SDK，而不是把喷涂逻辑写入基础 viewer。

## 12. 阶段性允许的妥协

以下妥协可以短期存在，但必须记录原因和退出条件：

- 某些 Qt glue 暂时源码开放，因为 ABI、QObject 生命周期或 plugin loader 尚未稳定。
- 工作模式包先静态链接进 `RobotQtViewer`，通过 CMake option 选择启用。
- 现有 `RobotQtModules` 继续作为过渡目录，直到迁移到一级包。
- 部分兼容 alias 暂时保留，避免一次性破坏现有 build。

不允许的妥协：

- 为了快速接入新包，在 `MainWindow` 增加大量业务硬编码。
- 让第三方扩展直接 include 基础包 private header。
- 同一业务命令在多个工作模式各写一套。
- 以预编译为理由冻结明显不足的公共接口，导致扩展只能改私有源码。

## 13. 与现有文档的关系

本文建立模块化 SDK 和软件包层面的长期准则。

相关已有文档：

- `docs/SDK_PACKAGING.md`：当前 `SMRobotCore` SDK 打包形态。
- `docs/SDK_EXTENSION_BOUNDARIES.md`：Core SDK、未来 ProjectSimulationSDK、VisualizationSDK 的边界。
- `docs/architecture/document_view_gui_contract.md`：GUI 文档-视图注册通知约束。
- `docs/architecture/simulation_platform_document_workbench_contract.md`：工作台模式、tree projection、action provider、panel resolver 和 overlay 的现有契约。

若旧文档与本文冲突，应以 `AGENTS.md`、当前源码和本文的项目级包化目标为准，并在执行计划中记录差异。
