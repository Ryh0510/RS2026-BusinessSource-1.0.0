# RobotQtViewer Viewport Interaction Mode 计划

## 目标

Viewport 不应该被每个 widget 或 controller 各自解释。它需要接受一个明确的交互模式，由当前 Workbench/TaskSession 决定当前允许选择什么、如何高亮、点击后产生什么 intent。

## 初始模式

当前已在 `RobotQtViewerWorkbenchManager` 中建立轻量枚举：

- `Browse`
- `SelectRobot`
- `SelectLink`
- `SelectMount`
- `SelectAttachment`
- `EditTransformPreview`
- `EditCollisionProxy`
- `SelectCollisionTarget`

这一步只建立 UI task 状态，不写入 `ProjectDocument`。

## 与 Workbench 的默认关系

- Browse workbench：默认 `Browse`
- Motion workbench：默认 `SelectRobot`
- ToolSetup workbench：默认 `SelectMount`
- Collision workbench：默认 `SelectCollisionTarget`

后续具体任务可以临时切换更细的模式，例如 ToolSetup 中编辑 TCP 时进入 `EditTransformPreview`。

## 设计规则

- Widget 只发出用户 intent，例如“我要选择 mount”。
- ModuleController 或 WorkbenchController 修改 `TaskSession`。
- Viewport 根据当前 `ViewportInteractionMode` 解释鼠标点击和 hover。
- Selection/highlight 的结果再通过 EventHub 或 ViewModel 更新其他 panel。

## 禁止事项

- 不把 task mode 写入项目文件。
- 不让多个 panel 同时直接控制 viewport selection。
- 不在 MainWindow 中新增复杂点击解释逻辑。

## 后续实现点

1. 给 `RobotViewport` 增加 `setInteractionMode(...)`。
2. AppController 订阅 Workbench/TaskSession 变化并同步 viewport。
3. SceneExplorer ViewModel 根据 interaction mode 设置可选节点与高亮策略。
4. ToolSetup、CollisionModel、CollisionRequest 分别只声明自己需要的选择模式。

## 验证

- Browse 模式能保持原有选择行为。
- ToolSetup 模式点击 mount/link 时只触发工具设置相关 intent。
- Collision 模式点击 link/attachment 时只触发碰撞目标 intent。
