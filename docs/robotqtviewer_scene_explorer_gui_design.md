# RobotQtViewer 左侧树 GUI 设计参考

本文记录当前 `RobotQtViewer` 左侧 `Scene Explorer` 的真实代码结构和设计边界，供后续修改
`SMRobotApps/RobotQtViewer/CMakeLists.txt`、左侧树逻辑、树节点展示或右键菜单时参考。

## 当前结论

- `SMRobotApps/RobotQtViewer/CMakeLists.txt` 只构建 viewer shell executable，并链接
  `RobotQtModulesSceneExplorer`；左侧树的主要逻辑不应继续直接写入 executable target。
- 左侧树模块位于 `SMRobotApps/RobotQtModules/SceneExplorer`，target 为
  `RobotQtModulesSceneExplorer`。
- `MainWindow` 负责创建 dock、连接信号、转发 viewport pick、显示 status message 和执行少量 shell
  级跨模块命令。
- 树项构造、节点 role、任务高亮、右键菜单模型、选择意图和 transform task panel 已经从
  `MainWindow` 迁出。
- Widget 不直接修改 `ProjectDocument`；持久修改通过 `SceneEntityWorkflowController`、
  `SceneEntityWorkflowController` 背后的 document workflow、`ToolSetupModuleController` 或
  `CollisionWorkbenchModuleController` 完成。

## CMake 和模块边界

```text
RobotQtViewer executable
    MainWindow
    RobotQtViewerSceneExplorerActionRouter
    shell adapters / toolbar / language / theme
    links RobotQtModulesSceneExplorer

RobotQtModulesSceneExplorer
    SceneExplorerWidget
    SceneExplorerModuleController
    SceneExplorerViewModel / SceneExplorerViewModelBuilder
    SceneTreeIntentController
    SceneSelectionController
    links RobotQtModulesShared
    privately links RobotQtModulesCoreWidgets
```

后续如果新增左侧树专用类，优先加入
`SMRobotApps/RobotQtModules/SceneExplorer/CMakeLists.txt`，而不是
`SMRobotApps/RobotQtViewer/CMakeLists.txt`。只有 shell 级组合代码、action router 或 viewer
入口文件才应加入 `RobotQtViewer` executable source list。

## 核心类职责

| 类 | 当前职责 | 不应承担 |
| --- | --- | --- |
| `SceneExplorerWidget` | 渲染左侧 `QTreeWidget`，应用 `SceneExplorerViewModel`，保存临时 UI 状态，发出节点点击和右键菜单信号 | 读取或持久修改 `ProjectDocument`，直接调用 viewport 或 project service |
| `SceneExplorerTaskWidget` | 渲染右侧 Task Panel 中的当前场景节点详情，显示 robot/object/point cloud transform 编辑器，显示点云 scale | 读取或持久修改 `ProjectDocument`，直接调用 viewport 或 project service |
| `SceneExplorerViewModelBuilder` | 从 `ProjectDocument`、robot/object runtime cache、当前选中节点和 viewport interaction mode 生成树视图模型 | 创建 `QTreeWidgetItem`，执行用户命令 |
| `SceneExplorerModuleController` | 持有 robot/object runtime cache，刷新 view model，处理 widget 信号，过滤当前任务下可选节点，转发选择和右键意图，调用 transform workflow | 拥有长期项目语义，直接实现跨模块业务 |
| `SceneTreeIntentController` | 从树 item 解析 `SceneExplorerNodeRef`，生成右键菜单动作模型，构造 collision selection set member | 执行右键动作 |
| `SceneSelectionController` | 把 `SceneExplorerNodeRef` 转成 selection intent，并解析 robot/link/mount 偏好 | 直接操作 `MainWindow`、viewport 或 widget |
| `RobotQtViewerSceneExplorerActionRouter` | 把右键菜单 action 分发到 ToolSetup、CollisionWorkbench、SceneExplorer transform task 或 shell callback | 构造树、读取树 role、修改树 UI |

## 数据来源

左侧树不是单纯的 `ProjectDocument` 镜像，而是 document 与 viewport/runtime 回调的组合视图。

```text
RobotViewport::robotLinksAvailable
    -> MainWindow::addRobotLinksToTree
    -> SceneExplorerModuleController::setRobotRuntime
    -> m_robots runtime cache

RobotViewport::sceneObjectAvailable
    -> MainWindow::addSceneObjectToTree
    -> SceneExplorerModuleController::setSceneObjectRuntime
    -> m_objects runtime cache

ProjectDocument
    -> robotMounts / mountedAttachments / pointClouds / attachmentAssets
    -> SceneExplorerViewModelBuilder
```

因此，机器人 link/joint 数组来自 viewport/runtime 回调；robot mount、mounted attachment、point cloud
和 tool asset 来自 `ProjectDocument`。刷新 viewport 时 `MainWindow::reloadViewportProject()` 会先
`SceneExplorerModuleController::clearRuntime()`，再由 reload 过程重新触发 runtime 回调。

## 树结构

当前 builder 生成这些顶层分组：

- `Robots`
- `Objects`
- `Point Clouds`
- `Tool Assets`

`Robots` 下每个机器人包含：

- robot 节点
- `Links (n)` 分组，link 下可展开 mounted mount 和 attachment
- `Joints (n)` 分组
- `Mounts (n)` 分组，按 mount 聚合 attachment

`Tool Assets` 当前过滤掉 `assetKind == "sensor"` 的资产，只展示非 sensor tool asset。

## 节点身份和 role

节点语义通过 `SceneExplorerNodeRef` 表达：

```cpp
SceneExplorerNodeKind kind;
QString id;
QString name;
QString linkName;
```

`SceneExplorerWidget::setDocumentView()` 将 view model 写入 `QTreeWidgetItem` role：

- `kSceneExplorerRoleId`
- `kSceneExplorerRoleName`
- `kSceneExplorerRoleType`
- `kSceneExplorerRoleLink`
- `kSceneExplorerRoleTaskSelectable`
- `kSceneExplorerRoleTaskHighlighted`

后续新增节点类型时，应同步更新：

- `SceneExplorerNodeKind`
- `kSceneExplorerNode...` 字符串常量
- `sceneExplorerNodeKindFromType(...)`
- `sceneExplorerNodeTypeName(...)`
- `SceneExplorerViewModelBuilder` 的节点生成和任务态规则
- `SceneSelectionController` 或右键菜单逻辑，如果新节点可被选择或操作

## 选择流程

```text
用户点击树节点
    -> SceneExplorerWidget::nodeActivated
    -> SceneExplorerModuleController::handleNodeActivated
    -> nodeSelectableForCurrentMode
    -> MainWindow::handleSceneExplorerNodeActivated
    -> SceneSelectionController::intentFromNode
    -> RobotQtViewerSelectionModel / RobotViewport / AppController / StatusBar
```

viewport 点选走并行入口：

```text
RobotViewport::scenePicked
    -> SceneSelectionController::nodeFromViewportPick
    -> MainWindow::handleSceneExplorerNodeActivated
```

这意味着树选中和 viewport 选中最终共享 `SceneSelectionIntent` 语义。后续改左侧树选择规则时，应优先改
`SceneSelectionController` 和 `SceneExplorerModuleController`，避免在 `MainWindow` 里新增节点类型判断。

## 任务态和高亮

`RobotQtViewerViewportInteractionMode` 会影响树节点是否可选、是否加粗高亮、以及 summary 文案。

当前逻辑由共享 helper 统一维护：

- `sceneExplorerNodeSelectableForMode()` 决定节点是否可选。
- `sceneExplorerNodeHighlightedForMode()` 决定节点是否加粗高亮。
- `SceneExplorerViewModelBuilder` 用共享 helper 生成展示态。
- `SceneExplorerModuleController::nodeSelectableForCurrentMode()` 用同一个共享 helper 做实际过滤。

后续若调整模式规则，应优先修改 `SceneExplorerViewModel.cpp` 中的共享 helper，避免显示态和实际行为不一致。

## 右键菜单流程

```text
用户右键树
    -> SceneExplorerWidget::treeContextMenuRequested
    -> SceneExplorerModuleController::showContextMenu
    -> SceneTreeIntentController::contextMenuModelFromItem
    -> QMenu
    -> SceneExplorerModuleController::contextMenuActionRequested
    -> MainWindow::handleSceneTreeContextMenuAction
    -> RobotQtViewerSceneExplorerActionRouter::handleAction
```

当前右键目标包括：

- `Robot`
- `Object`
- `PointCloud`
- `Link`
- `ToolAttachment`

当前动作包括：

- robot: edit robot mounts in Tool Setup、move base、delete robot
- object: move object、create tool asset from object、delete object
- point cloud: rename point cloud、move point cloud
- collision: add to new collision set、add to existing collision set

`RenamePointCloud` 仍由 `MainWindow::renamePointCloud()` 特判处理；其他动作进入
`RobotQtViewerSceneExplorerActionRouter`。如果新增右键动作，优先让
`SceneTreeIntentController` 只负责“菜单模型和 action payload”，让 action router 或所属模块 controller
负责执行。

## SceneExplorer Task Panel

右侧 Task Panel 中的 `SceneExplorerTaskWidget` 根据左侧树当前选择显示不同内容。`SceneExplorerWidget`
只负责左侧树；transform 编辑器不再放在左侧树下方。

可编辑目标：

- `Robot`: `RobotDesc::baseTransform`
- `Object`: `SceneObjectDesc::transform`
- `PointCloud`: `PointCloudDesc::transform`

点云缩放说明：

- `PointCloudDesc::scale` 是点云导入/加载 scale，默认值为 `1.0`。
- 新导入点云显式使用 `scale = 1.0`，默认不缩放。
- 当前 Task Panel 显示点云 scale，但不在该面板中编辑 scale。

流程：

```text
选择 robot/object/pointCloud
    -> ViewModelBuilder::applyTransformEditor
    -> SceneExplorerTaskWidget::setDocumentView

用户修改 transform
    -> transformPreviewChanged
    -> SceneExplorerModuleController
    -> RobotQtViewerViewportServices preview

用户 Apply
    -> SceneEntityWorkflowController set...Transform
    -> RobotQtViewerDocumentController::publishDocumentChanged
    -> EventHub
    -> SceneExplorerModuleController::refreshViewModel

用户 Cancel
    -> 从 ProjectDocument 取回原 transform
    -> viewport preview 恢复
    -> refreshViewModel
```

Transform preview 是 viewport 临时状态，Apply 才是 document mutation。

未提交 transform preview 的交互规则：

- 用户修改 transform 后，`SceneExplorerModuleController` 记录 pending preview 的目标节点和最新 transform。
- 用户继续刷新同一目标时，右侧编辑器继续显示 pending transform，`Apply Transform` 和 `Cancel Transform` 保持可用。
- 用户切换到其他树节点、右键切换到其他目标，或 viewport 点选其他目标时，未 Apply 的 preview 自动取消，并从
  `ProjectDocument` 恢复旧目标的已保存 transform。
- `Apply Transform` 成功后才写入 document 并清除 pending preview；`Cancel Transform` 恢复 document transform 并清除
  pending preview。
- 因此切换选择不会隐式保存 transform，也不会让 viewport 临时状态和右侧编辑器的 document 状态分离。

## EventHub 刷新

`defaultRobotQtViewerWindowConfig()` 中 `sceneExplorer` 订阅：

- `ProjectDocumentChanged`
- `ViewportReloaded`
- `RobotRuntimeChanged`
- `AttachmentChanged`
- `SelectionChanged`

`MainWindow::createPanels()` 通过 `RobotQtViewerDocumentViewRegistry::registerModule()` 注册
`sceneExplorer`，handler 调用 `SceneExplorerModuleController::handleEvent()`。controller 当前对
`ProjectDocumentChanged`、`ViewportReloaded`、`RobotRuntimeChanged`、`AttachmentChanged` 刷新 view model。

`SelectionChanged` 事件当前由 `MainWindow` handler 额外调用 `refreshSelectedLinkMaterialSummary()`，不是
`SceneExplorerModuleController::handleEvent()` 内部刷新路径。

## 修改左侧树时的建议

1. 新增展示节点：先扩展 `SceneExplorerViewModelBuilder` 和 `SceneExplorerViewModel`，再让
   `SceneExplorerWidget` 仅渲染。
2. 新增节点类型：同步维护 `SceneExplorerViewModel.cpp` 的 node kind/type 转换函数。
3. 新增可选节点：同步扩展 `SceneSelectionController`，不要在 `MainWindow` 里直接解析树 item。
4. 新增右键动作：在 `SceneTreeIntentController` 增加 action model，在
   `RobotQtViewerSceneExplorerActionRouter` 或所属模块 controller 中执行。
5. 新增持久项目修改：优先使用 `ProjectDocumentService`、`SceneEntityWorkflowController` 或对应模块
   command/facade；widget 不直接写 document。
6. 新增跨面板刷新：优先通过 `RobotQtViewerDocumentController` 发布 typed event，并在
   `RobotQtViewerWindowConfig` 中订阅。
7. 避免把新的 `QTreeWidgetItem` 构造、role packing、document schema 扫描重新写回 `MainWindow`。

## 当前风险点

- selectable/highlight 规则已抽到 `SceneExplorerViewModel.cpp`，后续风险主要是新增模式时忘记补共享 helper。
- node kind/type 双向转换已抽到 `SceneExplorerViewModel.cpp`，后续风险主要是新增节点类型时忘记补转换函数。
- `MainWindow` 仍有少量 SceneExplorer shell glue，例如
  `handleSceneExplorerNodeActivated()`、`renamePointCloud()` 和 delete confirmation。后续可继续逐步下沉，
  但不应为了单次左侧树调整做大规模迁移。
- viewport pick 到 `SceneExplorerNodeRef` 的映射已下沉到 `SceneSelectionController::nodeFromViewportPick()`；
  `MainWindow` 仍负责把 viewport 信号连接到选择落地流程。
- `RobotQtViewerSceneExplorerActionRouter::handleAction()` 的未使用 `basePath` 参数已清理。

## 验证建议

文档修改无需构建。后续真正改左侧树代码时，建议至少执行：

```bat
cmake --build build --config Debug --target RobotQtViewer
git --git-dir=D:/program/src/RS2026_CodexDev/.git --work-tree=D:/program/src/RS2026_CodexDev diff --check
```

若改动涉及 SceneExplorer target，也可以单独构建：

```bat
cmake --build build --config Debug --target RobotQtModulesSceneExplorer
```
