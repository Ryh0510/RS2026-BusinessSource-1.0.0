# Workbench GUI 与领域能力所有权清单

## 1. 清单目的

本清单源于 Workbench GUI 与领域包分离审计，记录类型、consumer、目标所有者和迁移边界。使用前应对照当前源码复核；长期约束以 `docs/architecture/workbench_gui_domain_package_separation.md` 为准。

## 2. CoreWidgets

| 类/文件 | 当前 consumer | 目标所有者 | 处理批次 |
|---|---|---|---|
| `RobotQtWidgetUtils` | Project Assembly、Collision、RobotRun、Status、MainWindow | `SMRobotWorkbenchCommon` | F1 保留并建立窄 common component |
| `ToolAssetEditorWidget` | `ProjectAssemblyToolSetup` | `SMRobotWorkbenchProjectAssembly` | F2 迁移 |
| `ToolTransformEditorWidget` | Project Assembly 的 SceneExplorer、ToolSetup | `SMRobotWorkbenchProjectAssembly` | F2 迁移；后续有第二个稳定 consumer 再评估 common |

结论：当前 `RobotQtModulesCoreWidgets` 不是纯 common，安装接口把 tool-specific editor 暴露给所有 Workbench consumer，应拆分。

## 3. Shared

### 3.1 保留在 Workbench common 的基础设施

- `RobotQtViewerDocumentContext`
- `RobotQtViewerDocumentController`
- `RobotQtViewerDocumentViewRegistry`
- `RobotQtViewerEventHub`
- `RobotQtViewerEvents`
- `RobotQtViewerSelectionModel`
- `RobotQtViewerViewportPreviewState`
- `RobotQtViewerWorkbench`
- `RobotQtViewerWorkbenchPackageRegistry`
- `RobotQtViewerWindowConfig`
- `InspectorContext`

这些类表达 Qt GUI 会话、document-view 通知、selection 和 Workbench 生命周期，不拥有规划、碰撞或喷涂算法。

### 3.2 暂时保留的跨模式 Qt application service

- `ProjectSessionWorkflowController`
- `RobotQtViewerAppController`
- `SceneEntityWorkflowController`
- `ViewportReloadWorkflowController`
- `RobotQtViewerViewportServices`

其中 `SceneEntityWorkflowController` 主要服务 Project Assembly，但目前由 `RobotQtViewerAppController` 聚合并由 app adapter 注入。F2 先迁移调用入口，再决定是否把实现完全移入 Project Assembly。

`RobotQtViewerViewportServices` 当前同时公开 Project Assembly、Collision 和 RobotRun 方法，是一个过宽的过渡接口。直接拆分会影响所有已迁移 Workbench 和 app adapter，因此应在各业务 Qt 组件闭包后再按 capability interface 分解。

### 3.3 移入 Collision Workbench

进入 `CollisionDetectorConfig`：

- `CollisionDetectorCommandController`
- `CollisionDetectorDocumentFacade`
- `CollisionDetectorsViewModel`
- `CollisionPairWorkflowController`
- `CollisionSelectionSetDocumentFacade`
- `CollisionSelectionSetsController`
- `CollisionSelectionSetsViewModel`

进入 `CollisionLinkModelSetup`：

- `CollisionLinkModelDocumentCommandController`
- `CollisionLinkModelDocumentFacade`
- `CollisionLinkModelVariantCommandController`

### 3.4 过渡保留项

- `CollisionRuntimeViewModel`

该文件本身使用标准 C++ 类型，不依赖 Qt，但被 `RobotQtViewerViewportServices` 的 public header 直接包含。当前先把它视为 viewport bridge contract；待宽型 viewport service 分成 capability interfaces 后，再迁入 Collision display component 或更合适的 Platform bridge。

## 4. Status

| 类 | 目标所有者 | 决策 |
|---|---|---|
| `StatusPanelWidget` | `RobotQtViewer` app shell | 保留全局状态展示；未来 RobotRun 专属运行监控另建 Workbench panel |

## 5. Dialog 和 MainWindow 调用

`SMRobotApps/RobotQtModules/Dialogs` 当前为空，不应继续作为业务 Dialog 集散目录。

| MainWindow 交互 | 目标所有者 |
|---|---|
| new/open/save/save-as/close dirty confirmation | app shell / project session workflow |
| import robot package、robot、object、point cloud | Project Assembly Workbench intent/controller |
| delete/rename scene entity confirmation | Project Assembly Workbench intent/controller |
| collision override sidecar、collision URDF export | Collision Config Workbench intent/controller |
| save viewport image | app shell viewport command |
| SDK diagnostic `loadRobot` | app shell diagnostic；后续可单独退役 |

Qt 标准 `QFileDialog`、`QMessageBox` 和 `QInputDialog` 没有待移动的本地源码；迁移对象是发起调用的业务入口。

## 6. RobotRun

`MotionControlWidget` 可以继续拥有 Qt 控件、显示单位和临时勾选状态。`MotionControlModuleController` 当前已经通过 `RobotQtViewerDocumentController::mutateProject` 修改初始姿态，符合正式 command API 路径。

仍需下沉的能力：

- joint value 的标准 C++ command contract；
- auto motion 的标准 C++ command contract；
- runtime snapshot 查询；
- 后续 trajectory start/pause/stop/step 状态机。

当前 `RobotRuntime` 只有 `RobotState`，`SimulationRuntime` 主要是 project/collision runtime，尚不存在完整 trajectory execution service。因此 F4 应先建立窄接口，不假设已有完整状态机。

## 7. Motion Planning

- `RobotTrajectoryCore`：已有非 Qt trajectory 数据与 sampler，可作为规划输出基础。
- `Kinematics`、`Collision`：可作为未来规划算法依赖。
- `SMRobotMotionPlanning`：尚不存在，应由 F5 新建。
- `SMRobotWorkbenchMotionPlanning`：当前只有 interface shell，应在 F6 改为 `SMRobotWorkbenchMotionPlanning` 并建立真实 Qt component。

## 8. 迁移顺序约束

1. 先拆 `CoreWidgets`，避免 Project Assembly editor 继续通过 common export 泄漏。
2. 再移动 collision facade/controller，但暂不移动 `CollisionRuntimeViewModel`。
3. 再建立 RobotRun 非 Qt command/service。
4. 建立 `SMRobotMotionPlanning` 后，才实现 Motion Planning Qt Workbench。
5. 最后拆宽型 viewport capability interface 和执行完整 prebuilt 矩阵。

该顺序避免 `SMRobotWorkbenchCommon` 反向依赖具体业务 Workbench。
