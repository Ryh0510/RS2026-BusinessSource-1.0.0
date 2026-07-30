# 机器人仿真平台文档与工作台模式架构契约

本文定义机器人仿真平台在底层仿真文档、GUI 会话文档、工作台模式、树/面板/视口联动上的长期架构约束。它用于指导后续 `RobotQtViewer`、`RobotGlfwViewer`、SDK、headless 工具和测试共同演进。

本文不是某一次重构的执行计划。仍有效的后续计划索引见 `plans/README.md`；具体实施前应按当前源码重新制定阶段计划。

## 1. 目标

- 保证机器人、场景、对象、绑定、轨迹、碰撞、喷涂、涂层等真实仿真语义不被 Qt GUI 私有化。
- 保证外部真实机器人接口、数字孪生映射、遥测状态、指令通道和安全联锁等真实运行语义不被 GUI mode 私有化。
- 允许 GUI 发起项目修改，但真实修改必须落到底层 project/session/runtime/service。
- 让 Qt GUI 拥有自己的会话文档，用于表达当前工作模式、选择、面板布局、显示策略和临时交互状态。
- 让工作模式成为组合器，而不是业务孤岛。
- 支持同一底层项目在不同 GUI 模式下呈现不同树、右键菜单、右侧面板和视口 overlay。

## 2. 两层文档

### 底层仿真文档 / 项目文档

底层仿真文档拥有真实项目事实和可运行语义，应该位于 `SMRobotCore` 或 `SMRobotPlatform` 的合适服务中。

它负责：

- robot、link、joint、tool、object、fixture、sensor、mount、ObjectFrame、attachment 等实体。
- 机器人结构、场景结构、对象加载、绑定关系、装配关系。
- collision object、collision group、collision pair、filter、distance/contact 请求。
- trajectory、motion state、runtime state、spray process、coating analysis 结果。
- external robot connection profile、controller endpoint、joint/TCP/IO mapping、telemetry snapshot、command session、heartbeat、latency、alarm 和 safety interlock 状态。
- project save/load、dirty policy、schema compatibility、SDK/headless 可见行为。

典型所有者包括：

- `SimulationProject`
- `ProjectSession`
- `ProjectDocumentService`
- `SimulationRuntime`
- `RobotRuntime`
- `RobotInstance`
- `Collision`
- 其他 Core/Platform service 或 command facade

底层仿真文档不应依赖 Qt widget、dock、dialog、tree item 或 GUI-only controller。

### GUI 会话文档

GUI 会话文档拥有 Qt 交互和显示会话状态。它可以影响 GUI 如何显示和如何解释用户意图，但不拥有真实仿真数据。

它可以负责：

- 当前 workbench mode。
- 当前 GUI selection、focus、hover、展开折叠、面板布局、dock 状态。
- 视口 highlight、preview、gizmo、overlay 的会话级显示策略。
- 当前模式下的可用 command、右键菜单、面板解析、tree projection。
- 尚未 Apply 的编辑草稿和可取消 preview。

它不应负责：

- 机器人结构本身。
- 对象绑定事实本身。
- 碰撞过滤事实本身。
- 轨迹、喷涂、涂层等运行或分析事实本身。
- 真实机器人连接、协议适配、遥测缓存、控制权限、安全确认和指令发送事实本身。
- save/load schema、dirty 规则、SDK/headless 行为。

## 3. GUI 修改底层项目的标准路径

GUI 可以修改底层项目，但必须通过底层文档、runtime 或 service 完成真实修改。正确流程是：

```text
Qt Widget / Panel / Viewport
    -> emit user intent
    -> ModuleController / WorkbenchModeController
    -> command / service / mutateProject(...)
    -> ProjectDocument / SimulationRuntime state change
    -> typed event / document changed notification
    -> GUI session update if needed
    -> ViewModel rebuild
    -> tree / panel / viewport repaint
```

禁止把“GUI 可以发起修改”理解为“widget 可以直接拥有项目语义”。GUI 层可以解释用户操作，但真实不变量必须由底层 project/session/runtime/service 维护。

## 4. WorkbenchMode 的职责

`WorkbenchMode` 是工作台组合器。它负责声明当前工作场景需要哪些视图、命令、选择策略和视口显示能力。

一个 mode 可以声明：

- 左侧使用哪些 tree projection。
- 右侧允许哪些 panel / inspector。
- 底部显示日志、运行控制、碰撞结果、轨迹时间轴或涂层统计。
- 当前可用 command group。
- 当前 selection policy。
- 当前 viewport overlay policy。
- 当前 context menu provider。

一个 mode 不应拥有：

- 机器人/对象/绑定/碰撞/轨迹的业务事实。
- 与其他模式重复的 domain command 实现。
- 私有 project schema。
- 私有保存/加载规则。
- 只有本模式知道的长期实体 ID。

如果某个规则会影响多个模式、保存加载、SDK、headless 或测试，它应上移到底层文档、runtime、service 或共享 view-model projection。

## 5. 当前模式分类

当前建议的工作模式划分如下。

### ProjectAssemblyMode

项目配置 / 建模装配模式。它包含当前阶段的机器人项目配置、连杆配置、物体加载、绑定/装配等能力。

典型职责：

- 导入机器人、对象、点云或其他场景实体。
- 编辑 robot/link/mount/ObjectFrame/object/attachment 属性。
- 配置对象绑定、装配层级和基础 transform。
- 显示项目树、装配关系、对象来源关系。

### CollisionConfigMode

碰撞检测配置模式。

典型职责：

- 配置 collision object、collision geometry、collision group、collision pair 和过滤规则。
- 查看距离、接触、碰撞请求和检测结果。
- 在视口中显示 collision overlay。

### RobotRunMode

机器人运行 / 仿真执行模式。

典型职责：

- 控制 runtime start/pause/stop/step。
- 查看关节状态、运行速度、当前 pose、运行日志。
- 显示运行态 highlight、trace 或状态 overlay。

### DigitalTwinMode

数字孪生 / 外部真实机器人接口模式。它用于把当前项目中的仿真机器人、工具、工件和工艺任务与外部真实机器人控制器建立受控映射，在 GUI 中观察真实状态、比较仿真与实机差异，并在满足安全策略时发起受限控制意图。

它不是“另一个仿真运行模式”。`RobotRunMode` 面向本地 `SimulationRuntime` 的仿真执行；`DigitalTwinMode` 面向外部控制器、真实设备状态和线上/离线同步边界。两者可以共享机器人实体、轨迹、碰撞和工艺语义，但连接生命周期、遥测来源、控制权限和安全联锁必须由底层 runtime/service/adapter 拥有。

典型职责：

- 选择或查看外部控制器连接配置、协议 profile 和设备身份映射。
- 建立项目 robot/link/joint/TCP/tool/base/object frame 与真实机器人 controller、joint、tool、workobject、IO、sensor 的映射关系。
- 显示连接状态、心跳、延迟、时钟同步、controller mode、alarm、fault、emergency stop、servo power、program state 等实时状态。
- 显示实机 joint/TCP pose、目标 pose、仿真预测 pose、跟随误差、轨迹偏差、IO/sensor 状态和关键安全区域 overlay。
- 发起经过权限、安全确认和 controller 状态检查的连接、断开、同步、校准、点动、轨迹下发、程序启动/停止等用户意图。
- 支持 dry-run、shadow mode、monitor-only、manual confirm、command preview 等上线前验证流程。
- 记录外部设备事件、指令回执、采样时间戳和诊断日志，供追溯、测试和 headless 工具复用。

必须由底层或平台服务拥有的事实：

- 外部 controller endpoint、认证材料引用、协议适配器、连接状态机和重连策略。
- 仿真实体到真实设备实体的稳定映射、坐标系标定、单位/方向/关节顺序转换。
- 实时遥测 snapshot、采样时间、质量标记、延迟估计和数据有效性。
- 控制权限、操作模式、安全互锁、急停/故障/报警、写指令允许条件。
- 指令队列、回执、失败恢复、日志、审计和可测试的 headless/API 行为。

GUI mode 可以拥有的会话状态：

- 当前选中的连接 profile、监控 tab、图表缩放、临时过滤器和展开状态。
- 本次连接向导中的未提交输入、命令预览、待确认对话框和 monitor-only 开关。
- 当前 overlay 显示策略，例如显示实际姿态、目标姿态、误差向量或安全区。

禁止事项：

- 不允许 widget 或 mode 直接调用 vendor SDK、socket 或现场总线接口发送真实机器人指令。
- 不允许把真实机器人状态只保存在 GUI 缓存中；遥测必须进入明确的 runtime/service snapshot。
- 不允许绕过底层安全联锁发送 jog、program start、trajectory upload 等会影响实机的命令。
- 不允许把连接密码、token 或本机私有路径写入普通 project schema；需要持久化时应使用受控 credential reference 或环境配置。
- 不允许为了适配某个品牌控制器，在 `RobotQtViewer` 中引入私有协议分支；应通过平台 adapter/service 抽象。

### TrajectoryPlanningMode

轨迹规划模式。

典型职责：

- 管理路径点、轨迹段、插补参数、可达性检查。
- 预览轨迹、编辑轨迹约束、显示规划结果。

### SprayProcessMode

喷涂工艺模式。

典型职责：

- 管理喷枪、喷涂区域、工艺参数、姿态约束、速度约束。
- 显示喷涂任务预览和工艺辅助 overlay。

### CoatingAnalysisMode

涂层分析模式。

典型职责：

- 显示涂层厚度、热力图、统计结果和质量分析。
- 支持结果筛选、查看和导出。

## 6. 防止状态爆炸

状态爆炸指每个 mode 都各自实现一套树、右侧面板、选择逻辑、右键菜单、命令和视口显示，导致同一概念在多个模式中重复且行为不一致。

必须避免以下模式：

- `ProjectAssemblyMode` 自己实现一套 object selection，`CollisionConfigMode` 又实现另一套不兼容 object selection。
- 同一个绑定命令在装配模式和喷涂模式各写一遍。
- 同一个 collision filter 规则同时散落在 tree、right panel 和 viewport。
- 树节点用 `QTreeWidgetItem` 的层级位置代表底层实体身份。
- 右侧面板直接根据树控件类型硬编码业务判断。

推荐原则：

- mode 复用底层实体语义、稳定 ID、选择机制、命令入口和 view-model 构建基础。
- mode 不要求复用完全相同的一棵树界面。
- mode 可以通过 tree projection 决定当前模式下的树形态。
- mode 可以通过 panel resolver 决定同一实体在不同模式下显示不同右侧面板。
- mode 可以通过 action provider 决定同一实体在不同模式下显示不同右键菜单。

## 7. 树模型复用与投影

左侧树应拆成以下层次：

```text
ProjectDocument / SimulationRuntime
    -> stable EntityRef / semantic node source
    -> mode-specific TreeProjection
    -> TreeViewModel
    -> Qt tree widget/view
```

`ProjectTreeViewModel` 不应被理解为一棵固定形状的树，而应理解为从底层项目生成树显示模型的一套机制。

不同模式可以共享：

- `EntityRef`
- entity kind
- entity id
- 基础图标/名称规则
- 基础选择映射
- 基础定位和展开能力
- command 输入中的实体引用

不同模式可以不同：

- 哪些节点可见。
- 节点如何分组。
- 是否显示装配关系节点。
- 是否显示 collision geometry / collision group。
- 右键菜单动作。
- 右侧面板解析。
- 视口 overlay。

### 示例：装配模式

```text
ProjectAssemblyMode
    TreeProjection:
        show robot / link / mount / object / ObjectFrame / attachment
        show assembly relation nodes
    ActionProvider:
        load object / bind / unbind / edit transform
    PanelResolver:
        RobotPropertyPanel / LinkPropertyPanel / ObjectPropertyPanel / BindingPanel
    ViewportOverlayPolicy:
        selection highlight / frame gizmo / attachment preview
```

### 示例：碰撞配置模式

```text
CollisionConfigMode
    TreeProjection:
        show robot / link / object / collision geometry / collision group
        hide assembly-only helper nodes unless needed for context
    ActionProvider:
        include in collision / exclude / add pair / inspect distance
    PanelResolver:
        CollisionObjectPanel / CollisionPairPanel / CollisionFilterPanel
    ViewportOverlayPolicy:
        collision overlay / distance markers / selected collision object highlight
```

### 示例：数字孪生模式

```text
DigitalTwinMode
    TreeProjection:
        show project robot / external controller / mapped joints / mapped tools / IO / telemetry channels
        group mapping status, live state and safety state separately
        hide assembly-only helper nodes unless needed for mapping context
    ActionProvider:
        connect / disconnect / resync / calibrate mapping / request jog / upload trajectory with safety confirmation
    PanelResolver:
        ExternalConnectionPanel / TelemetryPanel / MappingPanel / SafetyStatusPanel
    ViewportOverlayPolicy:
        actual pose vs simulated pose / target pose / following error / latency marker / safety zone
```

这些模式可以显示不同树，但同一个 link、object、robot 或 external controller 节点背后必须指向同一个稳定 `EntityRef` 或等价 runtime entity ref。

## 8. EntityRef 和节点身份

树节点身份必须来自底层实体引用，而不是 UI 层级位置。

推荐抽象：

```text
EntityRef
    kind: Robot / Link / Joint / Mount / Object / ObjectFrame / Attachment / CollisionObject / CollisionGroup / Trajectory / SprayRegion / CoatingResult / ExternalController / ExternalRobot / TelemetryChannel / SafetyInterlock
    id: stable document/runtime id
    role: optional display role or relation role
```

外部真实机器人相关节点也必须通过稳定引用表达。连接 profile、controller identity、mapping id、telemetry channel 或 safety interlock 可以来自 project document、runtime snapshot 或外部 adapter service，但不能来自树节点层级、显示文本或 widget 指针。

同一个对象在装配模式下可以显示为：

```text
Robot
    Link
        Mount
            Attached Object
```

在碰撞模式下可以显示为：

```text
Collision Objects
    Robot Links
    Scene Objects
```

但它背后仍应映射到同一个底层实体或 collision entity。命令、selection、highlight 和 right panel 解析必须基于 `EntityRef`。

## 9. ActionProvider 与右键菜单

右键菜单不应写死在 tree widget 中。tree widget 应只负责显示菜单并触发 action。

推荐流程：

```text
ContextMenuRequest
    currentMode
    selectedEntityRefs
    selectedNodeKinds
    document/runtime snapshot
    gui session state
        -> ActionProvider / ActionRegistry
        -> available actions
        -> command/service call
```

同一个 `Object` 在不同模式下可以有不同动作：

- `ProjectAssemblyMode`：绑定、解绑、编辑装配位姿、查看来源。
- `CollisionConfigMode`：加入碰撞检测、设置碰撞组、查看最近距离。
- `DigitalTwinMode`：映射实机通道、查看实时遥测、请求同步、发起受控点动或下发任务。
- `SprayProcessMode`：设为喷涂目标、分配工艺参数。

动作执行后必须通过底层 command/service 修改真实状态，再通过事件刷新视图。

## 10. PanelResolver 与右侧面板

右侧面板不应由 tree widget 直接硬编码。应由当前 mode、selection 和 document/runtime 状态共同解析。

推荐流程：

```text
SelectionChanged
    currentMode
    selectedEntityRefs
    document/runtime snapshot
    gui session state
        -> PanelResolver
        -> RightPanelViewModel
        -> widget setViewModel(...)
```

同一节点在不同模式下可以显示不同 inspector：

- `ProjectAssemblyMode + Link`：`LinkPropertyPanel`、`MountPanel`。
- `CollisionConfigMode + Link`：`CollisionGeometryPanel`、`CollisionFilterPanel`。
- `DigitalTwinMode + Robot`：`ExternalConnectionPanel`、`TelemetryPanel`、`SafetyStatusPanel`。
- `TrajectoryPlanningMode + Robot`：`TrajectoryTargetPanel`、`ReachabilityPanel`。

## 11. SelectionPolicy

选择状态要区分两类：

- 底层选择语义：选择了哪个 project/runtime entity。
- GUI 会话选择语义：当前模式如何解释该选择。

`SelectionPolicy` 负责判断：

- 当前 mode 允许选择哪些 entity kind。
- 多选是否允许。
- 选择后是否同步视口 highlight。
- 是否需要转换为 collision target、trajectory target、spray target 等任务语义。
- 是否需要转换为 digital twin mapping target、telemetry source 或 external command target。
- 选择变化是否需要触发 right panel rebuild。

长期选择语义不应只保存在某个 widget 中。

## 12. ViewportOverlayPolicy

视口 overlay 应由当前 mode 和 GUI 会话状态组合生成。

常见 overlay 包括：

- selection highlight
- frame gizmo
- attachment preview
- collision overlay
- distance/contact marker
- trajectory preview
- spray path preview
- coating heatmap
- live robot pose / target pose comparison
- telemetry trace
- safety zone / interlock status

overlay 可以是 GUI 临时显示，但如果它表达的是可保存项目配置、runtime 结果或分析结果，真实数据必须来自底层 document/runtime/service。

## 13. 新增或修改 Mode 的检查清单

新增或修改 mode 前，先回答：

- 这个 mode 的目标用户任务是什么？
- 它需要显示哪些底层实体？
- 它是否只是一个 tree projection，还是需要新的底层 project/runtime 语义？
- 它需要哪些 command group？
- 这些 command 是否已有底层 service？
- 它的右键菜单是否可由 ActionProvider 声明？
- 它的右侧面板是否可由 PanelResolver 解析？
- 它是否复用稳定 `EntityRef`？
- 它是否新增长期 GUI 会话状态？该状态是否会影响保存、加载、SDK 或 headless？
- 它需要哪些 viewport overlay？overlay 数据来自 GUI preview 还是底层 runtime/document？
- 如果 mode 会连接外部设备，它的连接、认证、遥测、控制权限和安全联锁是否已经有底层 service/adapter 表达？
- 任何会影响真实设备的 command 是否有权限检查、状态检查、人工确认、失败回滚或安全停止路径？
- 是否存在另一个 mode 已经实现相同 domain command？

如果答案显示需要新的真实仿真语义，先扩展底层 project/session/runtime/service，再接入 GUI。

## 14. 与其他文档的关系

- `docs/architecture/document_view_gui_contract.md` 定义 GUI 文档-视图注册通知与 ownership 总契约。
- 本文进一步定义 workbench mode、GUI 会话文档、tree projection、action provider、panel resolver 和 viewport overlay 的长期规则。
- 若旧文档与本文冲突，以 `AGENTS.md`、本文、`document_view_gui_contract.md` 和当前源码为准。

## 15. 局部聚焦 Viewport Preview 的触发边界

主视图局部聚焦是一种 task preview，不是普通 selection 行为。普通树节点选择只允许更新当前 selection、右侧面板、状态栏、普通 highlight 或 active item，不得隐藏完整场景，不得触发 `focusMountFrameLink`、`focusObjectFrameObject` 或 `focusMountedAttachment`。

当前允许触发局部聚焦的入口必须是明确 task：

- `Project Assembly` 中进入 mount frame / object frame 的创建或编辑任务。
- `Collision Config` 中通过左侧树右键 `Configure Collision Model...` 进入 `Collision Model Configuration` 任务。

这些 task 必须满足：

- 进入 task 时才发布局部聚焦 preview。
- task 内的 variant、frame transform、binding preview 可以继续刷新局部 preview。
- `Apply`、`Cancel` 或 task exit 必须清理局部聚焦并恢复完整场景。
- 普通 `refreshInspector()`、tree selection changed、panel view model rebuild 不得顺带发布局部聚焦 preview。

如果后续新增类似局部视图功能，必须先定义 task active/session state，再由 task controller 发布 preview；不得把局部聚焦放进通用刷新函数或普通 selection callback。
