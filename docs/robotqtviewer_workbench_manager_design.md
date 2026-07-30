# RobotQtViewer WorkbenchManager 设计

## 目标

`WorkbenchManager` 负责记录当前 UI 工作状态，不负责修改项目数据。

## 最小职责

- 记录当前 active workbench。
- 记录当前 viewport interaction mode。
- 提供 enter/exit 方法。
- 提供当前 task session。

## 不做的事情

- 不打开 dialog。
- 不调用 `ProjectDocumentService`。
- 不 reload viewport。
- 不直接操作 Widget。

## 第一阶段状态

第一阶段先支持：

- Browse
- Motion
- ToolSetup
- Collision

后续再拆出：

- CollisionModel
- CollisionRequest
- CollisionResult

