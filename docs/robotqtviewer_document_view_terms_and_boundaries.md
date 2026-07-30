# RobotQtViewer document-view 术语和边界

## 目的

本文用于固定 RobotQtViewer 后续模块化时的术语含义，避免 `Controller`、`Workflow`、`Service`、`ModuleController` 混用。

## ProjectDocument

`ProjectDocument` 是项目文件的数据模型。它应尽量保持为纯数据结构，用来表达机器人、场景物体、挂载点、附件、碰撞配置、视图状态等可保存内容。

允许职责：

- 表达项目 schema。
- 被 project IO 保存、加载、验证、迁移。
- 被 Qt viewer、GLFW viewer、headless runtime、SDK、测试共同读取。

禁止职责：

- 依赖 Qt。
- 持有窗口、控件、菜单、对话框。
- 直接管理运行时 viewport。

## ProjectSession

`ProjectSession` 是当前打开项目的一次编辑会话。它拥有一个 `ProjectDocument`，并额外记录文件路径、dirty 状态、是否需要 Save As，以及保存时路径便携化规则。

允许职责：

- 管理当前 document 生命周期。
- 管理当前项目路径。
- 管理 dirty / requiresSaveAs。
- 调用 project IO 进行 load/save。

禁止职责：

- 持有 Qt 控件。
- 表达 tool、sensor、collision 的长期业务规则。

## Service

`Service` 是非 UI 的数据操作服务。优先放在 `SMRobotPlatform` 或 `SMRobotCore`。

典型职责：

- 查找 project entity。
- 添加、删除、更新 project entity。
- 清理引用。
- 执行可被 Qt、GLFW、headless 测试共同使用的 project command。

## Workflow

`Workflow` 是一个较完整的业务流程。它可能组合多个 service 调用，也可能处理错误回滚、运行态刷新前置数据、事件发布前置数据。

判断规则：

- 不依赖 Qt 的 workflow，应优先下沉到 `SMRobotPlatform`。
- 依赖 `QFileDialog`、`QMessageBox`、`QMenu`、Widget signal/slot 的 workflow，可以留在 Qt app 层。
- 如果 workflow 未来也应该被 GLFW 或 SDK 使用，就不应长期留在 RobotQtViewer。

## Controller

`Controller` 是局部控制器。它通常做两类事情：

- 把 document/runtime 状态转换成 ViewModel。
- 把一个局部 command 应用到 service/document。

`Controller` 不要求和 Widget 一一成套出现。

## ModuleController

`ModuleController` 是 UI 功能模块的装配控制器。它连接 Widget intent、ViewModel builder、Workflow/Service、SelectionModel、EventHub。

允许职责：

- 订阅 Widget signal。
- 调用 ViewModel builder 刷新 Widget。
- 调用 service/workflow 执行用户 intent。
- 发布 document changed / selection changed 等事件。

禁止职责：

- 长期沉淀 tool/sensor/collision/robot 业务语义。
- 直接替代 `SMRobotPlatform` 的 project command。

## Widget

Widget 是纯 UI 组件。

允许职责：

- 创建控件。
- 管理控件内部 signal/slot。
- 接收 ViewModel 并显示。
- 发出用户 intent signal。

禁止职责：

- 直接修改 `ProjectDocument`。
- 直接保存/加载项目。
- 直接调用运行态重载。

## Dialog

Dialog 是短生命周期编辑窗口。它可以接收 edit model，允许用户编辑，然后返回编辑结果。

推荐规则：

- Dialog 尽量不长期持有完整 `ProjectDocument`。
- Dialog 不直接持久化修改。
- Dialog 只负责编辑输入，提交后由 ModuleController / Workflow / Service 应用到 document。

## ViewModel

ViewModel 是 Widget 可直接显示的数据快照。

允许职责：

- 包含 UI 显示所需文本、选项、启用状态、tooltip。
- 隔离 Widget 对 project schema 的直接依赖。

禁止职责：

- 持有可变 project 指针。
- 执行 project mutation。

