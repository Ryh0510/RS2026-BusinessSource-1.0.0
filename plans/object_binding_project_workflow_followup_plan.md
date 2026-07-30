# Object 绑定与 Project 保存工作流后续修改方案

## 目标

- 在主界面工具栏补齐 `Save As` 入口，保留已有菜单入口和 `Save Project As System`。
- 在 `New Project` 和 `Open Project` 替换当前项目之前检查未保存修改，支持 Save、Discard、Cancel；如果 Save As 被取消，则不继续替换项目。
- 让已经绑定到 Mount Frame 的源 Object 仍保留在 Scene Explorer 中，便于继续编辑 Object Frame 来校正安装间距。
- 增加 mounted attachment 的 unbind/detach 操作：从树上的 attachment 节点右键解绑，删除挂载关系，恢复可推断的源 Object 可见性，并清理不再使用的 attachment asset。
- 检查机器人绑定到机器人的扩展边界，避免把只有可视模型语义的 attachment 误用成完整机器人运行时挂载。

## 非目标

- 不重构 `RobotQtViewer` 的 document/view 架构。
- 不新增第三方依赖。
- 不修改旧 project IO schema，除非现有表达无法完成最小功能。
- 本轮不实现完整的机器人作为另一个机器人的子运行时实例；该能力需要独立 schema/runtime 设计。

## 需要检查的文件

- `SMRobotApps/RobotQtViewer/CMakeLists.txt`
- `SMRobotApps/RobotQtViewer/MainWindow.h`
- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.h`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerRibbonModel.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerSceneExplorerActionRouter.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneTreeIntentController.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerViewModelBuilder.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupModuleController.*`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocumentService.h`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`

## 预计修改的文件

- `SMRobotApps/RobotQtViewer/MainWindow.h/.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.h/.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerRibbonModel.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerSceneExplorerActionRouter.cpp`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneTreeIntentController.h/.cpp`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerViewModelBuilder.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupModuleController.h/.cpp`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocumentService.h`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`
- `docs/agent_change_audit.md`

## 主要类和函数

- `MainWindow::newProject`
- `MainWindow::openProject`
- `MainWindow::saveProjectAs`
- 新增 `MainWindow::confirmProjectReplacement`
- `RobotQtViewerToolbarController::makeActionMap`
- `makeDefaultRobotQtViewerRibbonModel`
- `SceneTreeIntentController::contextMenuModelFromItem`
- `RobotQtViewerSceneExplorerActionRouter::handleAction`
- `buildSceneExplorerViewModel`
- 新增 `ToolSetupModuleController::unbindMountedAttachment`
- 新增 `ProjectDocumentService::removeMountedAttachment`
- 新增 `ProjectDocumentService::removeAttachmentAssetIfUnused`

## 预期行为

- 用户可以从顶部工具栏直接另存为当前项目。
- 当前 project dirty 时点击 New/Open 会弹出确认；选择 Save 会执行保存，保存取消或失败时中止 New/Open；选择 Discard 才丢弃当前修改。
- 绑定后的源 Object 在树中仍可见，并标注为 bound/hidden，Object Frame 子节点仍可右键编辑。
- mounted attachment 节点右键出现 `Unbind Attachment...`；确认后删除挂载，恢复与 attachment asset `visualPath` 匹配的隐藏源 Object，并刷新视口和 Scene Explorer。
- unbind 会移除 collision selection set 中引用该 attachment 的成员；若 asset 没有被其他 mounted attachment 引用，则删除 asset。

## 验证方法

- 配置或复用现有 build 目录。
- 构建 `RobotQtViewer` 目标。
- 人工检查：工具栏 Save As 出现；dirty project 的 New/Open 提示；绑定后 Object Frame 仍能编辑；右键 attachment 能解绑并恢复源 Object。

## 最小性说明

- 保存工作流只在 app shell 的入口增加确认，不改变 `ProjectSession` 的存储语义。
- 绑定后 Object Frame 可编辑只调整 Scene Explorer view model，不改变绑定 schema。
- unbind 的持久修改下沉到 `ProjectDocumentService`，Qt 层只发起用户 intent 和刷新。
- 机器人绑定机器人不在本轮强行编码，因为当前 `MountedAttachmentDesc` 只表达 attachment asset 可视/功能框架，不表达子机器人运行时、关节状态、碰撞、选择和保存恢复。

## 停止条件

- 如果需要修改超过 12 个源/配置文件，停止并汇报。
- 如果完整机器人绑定必须新增 project schema 或 runtime ownership，停止该子项并输出后续设计建议。
- 如果两次构建因不同原因失败，停止并汇报。
- 如果 unbind 无法可靠恢复源 Object 且需要新增迁移字段，停止并说明风险。
