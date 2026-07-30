# CollisionConfigMode 实现设计说明

本文记录 `CollisionConfigMode` 在当前 `RobotQtViewer` 工作台模式框架下的实现方式和后续演进边界。它的目标是作为后续开发参考，避免把碰撞配置继续做成独立 UI 孤岛。

## 1. 目标与非目标

### 目标

- 将碰撞配置作为 `WorkbenchMode` 的一个模式接入，而不是作为散落在菜单、面板、树和 viewport 中的独立状态。
- 复用当前 `ProjectAssemblyMode` 已有形式：`RobotQtViewerWorkbenchDescriptor -> SceneExplorer projection/action scope -> right panel -> viewport interaction mode`。
- 保留现有 `CollisionDetectorConfig` 和 `CollisionLinkModelSetup` 能力，并把它们组织为 `CollisionConfigMode` 的右侧配置任务；运行结果显示不属于该配置模式的右侧面板。
- 明确碰撞真实语义的所有权：项目配置属于 `SimulationProject` / `ProjectDocumentService`，运行检测属于 Collision runtime / viewport runtime，Qt 只做用户意图适配。
- 为后续 `TrajectoryPlanningMode`、`SprayProcessMode`、`CoatingAnalysisMode` 预留同样的 descriptor / toolbar / projection 扩展路径。

### 非目标

- 本文不要求重写当前碰撞算法、FCL 封装、URDF collision override 或 detector runtime。
- 本文不引入新的 project schema。当前 `ProjectDocument::collision` 已能表达 selection set、detector、override 和 visualization。
- 本文不把 `MainWindow` 变成碰撞业务控制器。`MainWindow` 只负责 shell/composer 连线。
- 本文不要求一次性拆完旧的 collision panel；旧模块应通过模式入口逐步收口。

## 2. 设计原则

`CollisionConfigMode` 是工作台组合器，不是碰撞业务所有者。

正确数据流应保持为：

```text
Toolbar / tree / panel user intent
    -> MainWindow shell enters CollisionConfigMode or routes action
    -> CollisionWorkbenchModuleController / SceneTreeIntentController
    -> ProjectDocumentService / Collision command facade / viewport service
    -> ProjectDocument or runtime state changes
    -> RobotQtViewerEventHub / RobotQtViewerDocumentController events
    -> SceneExplorer / Collision panel / viewport refresh
```

需要持久保存、SDK/headless 可见或多视图共享的状态必须在 `SimulationProject`、`SimulationRuntime`、`Collision` 或相关 service 中表达。Qt widget 可以保存当前 tab、编辑框内容、hover、未 Apply 草稿等纯 UI 状态。

## 3. 当前代码盘点

### 工作台模式入口

- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerWorkbench.h`
- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerWorkbench.cpp`

当前模式 descriptor 已表达：

- `kind`
- `domain`
- `rightPanel`
- `defaultViewportMode`
- `displayName`
- `rightPanelTitle`
- `projectAssemblyTreeProjection`
- `collisionTreeProjection`
- `projectAssemblyActions`
- `collisionActions`

`CollisionConfigMode` 对应现有 `RobotQtViewerWorkbenchKind::Collision`，其 descriptor 应保持：

```text
domain: CollisionConfig
rightPanel: CollisionConfig
defaultViewportMode: SelectCollisionTarget
tree projection: CollisionConfig
action scope: CollisionConfig
```

### 工具栏模式选择

- `SMRobotApps/RobotQtViewer/RobotQtViewerRibbonModel.*`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.*`
- `SMRobotApps/RobotQtViewer/MainWindow.*`

模式选择应是互斥 action group。当前应启用：

- `Project Assembly`
- `Collision Config`
- `Robot Run` 如沿用已有 motion 面板，可作为已有模式继续保留。

后续模式先保留禁用入口：

- `Trajectory Planning`
- `Spray Process`
- `Coating Analysis`

这些禁用入口只表达 UI 扩展点，不代表底层能力已经实现。

### 左侧树投影与右键动作

- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerViewModel.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerViewModelBuilder.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneTreeIntentController.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerModuleController.*`

当前 `SceneExplorerTreeProjection::CollisionConfig` 已作为最小投影入口存在。它不应复制实体身份系统，应继续复用 `SceneExplorerNodeRef`，后续可提升为更通用的 `EntityRef`。

当前 `SceneExplorerActionScope::CollisionConfig` 已把右键菜单收口到碰撞动作，例如：

- `Add to New Collision Set...`
- `Add to Collision Set...`

后续应扩展为正式 `ActionProvider`，但命令仍应路由到 collision controller / service，不应由 tree widget 直接改 document。

### 右侧碰撞面板

- `SMRobotApps/RobotQtModules/CollisionDetectorConfig`
- `SMRobotApps/RobotQtModules/CollisionLinkModelSetup`

当前 `CollisionWorkbenchModuleController` 组合了：

- `CollisionDetectorConfigModuleController`
- `CollisionLinkModelSetupModuleController`

它已经接近 `CollisionConfigMode` 的 mode-specific panel controller。右侧默认只显示 detector 配置；只有左侧树节点右键触发“配置碰撞模型”时，才临时替换为 collision model 配置任务。配置模式不得提供常驻多页导航，也不得把运行结果页面放入该右侧面板。

### ProjectDocument 碰撞数据

- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocumentService.h`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`

当前 `ProjectDocument::collision` 包含：

- `CollisionSelectionSetDesc`
- `CollisionSelectionSetMemberDesc`
- `CollisionDetectorDesc`
- `CollisionDetectorTargetDesc`
- `CollisionPairGeneratorDesc`
- `CollisionPairFilterDesc`
- `CollisionVisualizationDesc`
- `RobotCollisionOverrideDesc`
- `ObjectCollisionOverrideDesc`
- `AllowedCollisionPairDesc`

这些是碰撞配置事实的所有者。Qt 面板只能通过 `ProjectDocumentService` 或已有 command facade 修改它们。

### Runtime / viewport 碰撞能力

- `SMRobotApps/RobotQtViewer/RobotQtViewerViewportServicesAdapter.*`
- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerViewportServices.h`
- `SMRobotPlatform/SimulationRuntime/src/ProjectSimulationRuntime.cpp`
- `SMRobotCore/Collision`
- `SMRobotPlatform/RobotRenderBridge`

viewport service 已提供：

- collision geometry visibility
- active detector
- detector enabled / visible
- detector runtime options update
- rebuild detectors from document
- collision runtime detector summaries
- robot collision proxy generation / quality evaluation

`CollisionConfigMode` 的 overlay policy 应调用这些服务，不应在 widget 中直接保存长期 viewport 碰撞状态。

## 4. CollisionConfigMode 目标结构

### Descriptor

`CollisionConfigMode` 应由 descriptor 声明，而不是由多个 switch 分散判断：

```text
RobotQtViewerWorkbenchDescriptor
    kind = Collision
    domain = CollisionConfig
    rightPanel = CollisionConfig
    defaultViewportMode = SelectCollisionTarget
    collisionTreeProjection = true
    collisionActions = true
```

后续如果需要更强表达，可把布尔字段替换为枚举：

```text
treeProjection: ProjectAssembly / CollisionConfig / TrajectoryPlanning / SprayProcess / CoatingAnalysis
actionScope: ProjectAssembly / CollisionConfig / ...
panelResolver: ProjectAssembly / CollisionConfig / ...
overlayPolicy: ProjectAssembly / CollisionConfig / ...
```

短期保留布尔字段是为了最小改动；新模式增加时应优先迁移为枚举，不要继续增加多组布尔。

### Toolbar

工具栏模式组应只表达“进入哪个 workbench mode”：

```text
Modes
    Project Assembly
    Collision Config
    Robot Run
    Trajectory Planning (disabled)
    Spray Process (disabled)
    Coating Analysis (disabled)
```

`Project Assembly` 进入当前 `Browse` descriptor；具体“添加 Mount Frame”等命令保留在编辑组或右键菜单中。这样可以区分“模式”与“命令”。

### TreeProjection

`CollisionConfigMode` 的树应优先展示能成为 collision target 的实体：

- robot
- link
- mount / attachment
- scene object
- object frame
- point cloud 如未来支持碰撞代理
- collision selection set / detector 如后续需要树内管理

当前实现可以先复用现有 project tree，并隐藏装配专用 helper：

- 不显示 `Tool Assets` 这类装配资产分组。
- 除非用于上下文，不显示 source frame helper。
- 保留稳定 `SceneExplorerNodeRef`，保证同一个 link/object 在不同模式中仍指向同一底层实体。

后续建议新增明确节点类型：

```text
CollisionSelectionSet
CollisionDetector
CollisionGeometry
CollisionOverride
AllowedCollisionPair
```

新增前先确认是否需要树中管理；如果右侧面板已经能完整管理 detector/selection set，树可以只显示可选 target。

### ActionProvider

`CollisionConfigMode` 下右键菜单应只出现碰撞配置动作。当前已有最小动作：

- Add to New Collision Set
- Add to Collision Set

后续可扩展：

- Remove from Collision Set
- Create Detector From Selection
- Show Only This Collision Target
- Inspect Nearest Distance
- Include / Exclude Collision Pair
- Generate Collision Proxy

动作执行规则：

- 只读动作可以直接查询 document/runtime snapshot。
- 持久配置动作必须通过 `mutateProject(...)` + `ProjectDocumentService`。
- runtime-only 显示动作必须通过 viewport service，并明确 dirty policy。
- 任何 action 不应在 `QTreeWidgetItem` 层直接拼装业务状态。

### PanelResolver

当前 `CollisionWorkbenchPanel` 可以继续作为 `CollisionConfigMode` 的右侧主面板。后续应逐步把“选择实体 -> 显示哪个子面板或 view model”的规则抽出：

```text
CollisionConfigMode + Robot
    -> Robot collision model summary
    -> detector list filtered by robot

CollisionConfigMode + Link
    -> link collision geometry variants
    -> generate proxy / use variant in active detector

CollisionConfigMode + Object
    -> object collision override panel
    -> add primitive / replace original collision

CollisionConfigMode + SelectionSet
    -> member list / rename / remove

CollisionConfigMode + Detector
    -> detector properties / visualization controls
```

短期可以由 `CollisionWorkbenchModuleController` 内部调度；长期应形成 `CollisionPanelResolver` 或等价 view-model builder。

### SelectionPolicy

`SelectCollisionTarget` 当前允许选择：

- robot
- link
- robot mount
- attachment
- object
- object frame
- point cloud

后续应补齐两类语义：

- `EntityRef`：底层实体身份，跨模式稳定。
- `CollisionTargetRef`：碰撞任务语义，由 `EntityRef` 转换而来，例如 robot link、scene object、attachment。

转换逻辑不应散在 tree、right panel、viewport 中。建议放在共享 helper 或 `CollisionTargetResolver`：

```text
SceneExplorerNodeRef / EntityRef
    -> CollisionSelectionSetMemberDesc
    -> CollisionDetectorTargetDesc / pair generator input
```

### ViewportOverlayPolicy

`CollisionConfigMode` 默认 overlay 应包含：

- collision geometry 可见性。
- active detector highlight。
- contact / nearest point / normal marker。
- selected collision target highlight。
- proxy generation preview。

overlay 数据来源分类：

- 持久配置：`ProjectDocument::collision.*`
- runtime result：Collision runtime / `RobotQtViewerViewportServices`
- 临时预览：GUI task session 或 viewport preview state，Apply 后必须落到 document 或清理

不要把“某 detector 可见”“当前 active detector”“collision geometry 总开关”散落为多个 widget 私有状态。当前已有 viewport service 能表达的，应继续通过 service 进入。

## 5. 事件与刷新

当前可复用机制：

- `RobotQtViewerDocumentController::mutateProject(...)`
- `RobotQtViewerDocumentController::publishCollisionChanged(...)`
- `RobotQtViewerDocumentController::publishDocumentChanged(...)`
- `RobotQtViewerEventHub`
- `RobotQtViewerDocumentViewRegistry`
- `CollisionWorkbenchEventCoordinator`

推荐刷新链：

```text
Collision panel command
    -> mutateProject(sourceId, dirtyPolicy, mutation)
    -> ProjectDocumentService changes ProjectDocument::collision
    -> publishCollisionChanged / publishDocumentChanged
    -> CollisionWorkbenchModuleController::handleEvent
    -> refresh detector/selection/result/model view
    -> SceneExplorer receives document event and rebuilds projection if needed
    -> viewport reload/update service if runtime detector changed
```

如果只改变 runtime overlay，例如 show only selected detector，可用 runtime service 更新 viewport，但仍应明确是否需要 document dirty。

## 6. 迁移步骤建议

### 阶段 1：模式入口收口

- 工具栏使用 `Modes` 互斥按键群。
- `Project Assembly` 与 `Collision Config` 可切换。
- `Collision Config` 切换后驱动 collision tree projection、collision action scope、collision right panel、`SelectCollisionTarget` viewport mode。

验收：

- 切换 `Project Assembly` 后右键菜单显示装配动作。
- 切换 `Collision Config` 后右键菜单只显示碰撞动作。
- 右侧面板切到 `CollisionWorkbenchPanel`。

### 阶段 2：TreeProjection 强化

- 保留稳定 node ref。
- 增加 collision-specific grouping，例如 `Collision Targets`、`Robots`、`Scene Objects`。
- 根据需要引入 `CollisionSelectionSet` / `CollisionDetector` 节点。

停止条件：

- 需要新增持久 schema 时停止，先设计 `SimulationProject` 字段。
- 树投影开始复制 panel 中已有 detector 管理逻辑时停止，先定义 ownership。

### 阶段 3：ActionProvider 正式化

- 将 `SceneTreeIntentController` 中的 collision action 判断抽成独立 provider 或 action registry。
- action 输入统一使用 `EntityRef` / `CollisionTargetRef`。
- action 执行统一路由到 collision controller 或 `ProjectDocumentService`。

### 阶段 4：PanelResolver 正式化

- `CollisionWorkbenchPanel` 保留为容器。
- 根据当前 selection 构建 `CollisionRightPanelViewModel`。
- 不再由 MainWindow 或 tree widget 直接决定子面板业务。

### 阶段 5：OverlayPolicy 正式化

- 将 active detector、selected target highlight、distance/contact marker 显示策略集中到 `CollisionViewportOverlayPolicy` 或等价 descriptor。
- Apply/Cancel preview 与 document/runtime 刷新统一由 controller 管理。

## 7. 测试与验证

优先验证顺序：

1. `cmake --build build --config Release --target RobotQtViewer`
2. `cmake --build build --config Release --target RobotQtModulesCollisionDetectorConfig`
3. `cmake --build build --config Release --target RobotQtModulesCollisionLinkModelSetup`
4. 运行已有 `SimulationProject` regression，覆盖 selection set / attachment cleanup。
5. 手动 UAT：
   - Project Assembly / Collision Config 模式切换。
   - 树投影和右键菜单切换。
   - 添加 collision selection set member。
   - 创建 detector、启停 detector、切换 detector visibility。
   - 生成 collision proxy 并保存 override。
   - 关闭/打开项目后 collision 配置保持一致。

## 8. 风险与约束

- 当前 `SceneExplorerNodeRef` 还不是完整 `EntityRef`。在 trajectory/spray/coating 加入前，应评估是否抽出通用 `EntityRef`。
- 当前 `RobotQtViewerWorkbenchDescriptor` 用布尔字段表达 projection/action scope。未来模式增加时应改为枚举字段，避免布尔组合爆炸。
- 当前 `CollisionWorkbenchPanel` 内部包含多个旧 controller。短期可接受，长期应逐步形成 mode-specific resolver。
- 不应把 collision filter、detector pair generator 或 collision override 逻辑复制到 `MainWindow`、tree widget 或 toolbar。
- collision visualization 可能既有 document 字段又有 runtime 状态。每次新增开关都必须明确 dirty policy。

## 9. 推荐下一步

- 完成 `CollisionConfigMode` 的 tree projection 分组设计：先只处理 target 展示，不把 detector 管理搬进树。
- 增加 `CollisionTargetResolver`，把 `SceneExplorerNodeRef -> CollisionSelectionSetMemberDesc` 转换集中化。
- 把 `SceneTreeIntentController` 的 collision actions 抽成 `CollisionActionProvider`。
- 为 selection set member 增删补充非 GUI regression，验证通过 service 修改 document。
