# RobotQtViewer Workbench / TaskMode 术语

## Shell

`Shell` 是 RobotQtViewer 的外壳，负责菜单、工具栏、dock、状态栏、viewport 和模块装配。Shell 可以打开 Qt 对话框，也可以切换当前 task，但不应该拥有 robot/tool/collision 的长期业务语义。

## Workbench

`Workbench` 是一组围绕某类任务组织的 UI 和交互模式。例如：

- Home / Browse
- ToolSetup
- CollisionModel
- CollisionRequest
- Motion

Workbench 不是数据模型。它描述的是当前用户在界面中做什么。

## TaskMode

`TaskMode` 是当前交互状态。比如进入 ToolSetup 后，左侧树、右侧 panel、viewport 拾取/高亮都应围绕工具设置服务。

## TaskSession

`TaskSession` 是临时 UI 编辑状态，记录当前任务目标和是否允许退出。它可以记录：

- 当前 task id。
- 当前 robot/link/mount/attachment/detector。
- 当前是否 dirty。
- 当前是否需要 apply/cancel。

它不应写入 `ProjectDocument`。`ProjectDocument` 只保存长期项目数据。

## TaskPanel

`TaskPanel` 是右侧当前任务的工作面板。它替代过去“所有内容都在 Inspector tabs 里”的思路。

## ScenePanel

`ScenePanel` 是左侧项目树或选择树。它可以根据当前 task 切换 ViewModel 过滤和可选对象，但 Widget 本身仍然只显示 ViewModel。

## RibbonCommand

`RibbonCommand` 是顶部命令入口。第一阶段不引入第三方 ribbon 控件，先用 Qt 原生 `QToolBar` 和 checkable `QAction` 模拟。

## ViewportInteractionMode

`ViewportInteractionMode` 是 viewport 当前允许的拾取和高亮模式，例如 Browse、SelectLink、SelectMount、EditCollisionProxy。第一阶段只记录状态，不强行改造所有拾取逻辑。

## 与旧 Inspector 的关系

`Inspector` 可以保留为只读摘要或属性查看概念，但不再作为复杂任务的主架构名称。复杂任务应进入对应 Workbench。

