# GUI 文档-视图注册通知架构契约

本文定义 RobotQtViewer 以及后续 GUI 模块修改时必须遵循的文档-视图结构。它不是某一个窗口的实现计划，而是判断 GUI、绑定、显示、选择、视口刷新等代码应该放在哪一层的设计契约。

## 1. 核心原则

所有可被用户理解为项目状态、运行状态或显示策略的 GUI 结果，都应由数据结构表达，再由视图根据数据结构显示。

标准流程是：

```text
User Intent
    -> Message / Command / Mutation
    -> Document / Session / Runtime State Change
    -> Registered View Notification
    -> ViewModel Rebuild
    -> Widget / Dialog / Viewport Repaint
```

不应通过直接调整多个 widget、tree item、dock、dialog 或 viewport flag 来制造最终显示效果。直接调整 UI 只允许用于局部临时状态，例如焦点、hover、高亮、编辑框尚未 Apply 的文本、拖拽中预览、窗口尺寸等。

## 2. 所有权边界

### Document / Session

Document/session 拥有项目事实：

- robot、object、mount、ObjectFrame、attachment、sensor、fixture 等实体。
- 绑定关系、来源关系、显示策略、保存/加载字段、dirty 规则。
- 可以被保存、恢复、导出、验证、undo/redo 或非 Qt 工具使用的状态。

这些状态必须通过 `ProjectSession`、`ProjectDocumentService`、`SimulationProject`、`SimulationRuntime` 或其他 Core/Platform 服务修改。

### Runtime

Runtime 拥有执行态：

- 当前机器人实例状态。
- 运行中的 trajectory / collision / sensor simulation 状态。
- 从 document 构建出的 runtime entity 和可重建缓存。

Runtime 状态变化也应通过明确事件通知视图，而不是由某个 widget 私自保留长期副本。

### ViewModel

ViewModel 是 document/runtime 到 UI 的显示映射。它可以：

- 选择哪些数据展示。
- 决定树的层级、面板字段、按钮 enabled/visible 状态。
- 将 domain data 转换为 Qt-friendly 文本、列表、表格、颜色或图示数据。

ViewModel builder 不应修改持久 document，也不应调用会产生项目副作用的 service。

### Widget / Dialog / Viewport

Widget、dialog、viewport 是视图或交互适配器。它们可以：

- 接收 view model 并显示。
- 发出用户意图信号。
- 保存纯 UI 临时状态。
- 显示错误、状态文本、焦点和布局。

它们不应拥有：

- 项目实体语义。
- 绑定关系。
- 持久显示策略。
- 保存/加载规则。
- 跨视图选择语义。
- 可以影响 headless、SDK、RobotGlfwViewer 或测试的行为。

## 3. 注册通知机制

文档本身不应硬编码知道哪些 GUI 对象存在。GUI 对象创建后，应由其 controller 或模块注册到相关 document/context/event registry。

推荐形态：

```text
ModuleController / DialogController
    registers with DocumentViewRegistry / EventHub / DocumentController
    declares interested events
    receives event payload or document snapshot
    rebuilds view model
    updates widget
```

已有 Qt app 层机制包括：

- `RobotQtViewerDocumentController`
- `RobotQtViewerEventHub`
- `RobotQtViewerDocumentViewRegistry`
- `RobotQtViewerDocumentContext`
- 各 `RobotQtModules::*ModuleController`
- `RobotQtViewerSelectionModel`

新增 GUI 区域时，应优先复用这些机制。如果缺少合适事件，应添加最小 typed event 或注册入口，而不是让 MainWindow 或某个 widget 手动调用多个无关控件刷新。

## 4. 禁止模式

以下模式默认禁止：

- 在 widget 回调中直接修改 `ProjectDocument`。
- 在 MainWindow 中添加新的 project schema role packing 或持久 `QTreeWidgetItem` 构造逻辑。
- 一个按钮点击后直接操作多个 widget/viewport 来“修补显示”，但 document 或 runtime 状态没有表达该变化。
- 同一显示规则散落在 tree、right panel、viewport、dialog 中各自判断。
- 新增长期 GUI 状态来替代 document 字段或 runtime 状态。
- 为了让某个面板看起来正确而绕过 `mutateProject(...)`、command/service 或 event hub。
- 让 widget 直接订阅底层事件并同时拥有业务判断；应由 module/controller 代表 widget 订阅并构建 view model。

## 5. 允许的局部 UI 状态

以下状态可以留在 UI 层：

- 当前输入框文本，直到用户 Apply/Cancel。
- hover、focus、selection highlight 的纯视觉效果。
- splitter、dock、scroll position、展开折叠状态。
- 拖拽中或编辑中的预览状态，但 Apply 后必须通过 command/mutation 写入正确 document/runtime 状态，Cancel 后必须清理。
- 对话框内部临时 edit model，但最终提交必须走 command/service。

如果临时状态会影响保存、加载、其他视图、SDK、headless 工具或 runtime 行为，它就不再是纯 UI 状态。

## 6. GUI 修改前检查

修改 GUI 或绑定显示前，先回答：

- 这个行为是纯 UI 状态，还是项目/运行/显示语义？
- 如果关闭再打开项目，是否需要恢复？
- 如果用 RobotGlfwViewer、SDK、测试或 headless runtime，是否也应该成立？
- 这个状态是否影响 tree、right panel、viewport、collision、selection、save/load 中多个视图？
- 当前 document/runtime 是否能表达它？
- 哪个 command/mutation 应该改变它？
- 哪些 view 通过注册机制接收通知？
- 哪个 view model builder 应该产生最终显示？

如果答案指向项目或运行语义，应先修 document/runtime/service，再改视图。

## 7. 绑定与装配显示规则

机器人系统中的 mount、ObjectFrame、attachment、tool、sensor、fixture 等绑定关系，应优先作为 document/runtime 关系表达。视图只能根据这些关系显示装配层级。

例如 Object 绑定到 Mount Frame 后，显示顺序应由绑定关系表达：

```text
Robot / Link
    -> Mount Frame
        -> Object Frame or Object origin
            -> Object / Attachment visual
```

如果 UI 需要显示不同顺序、隐藏坐标编辑器、显示只读绑定关系图或显示可编辑面板，应由 view model 根据 document 中的绑定来源、frame 来源、编辑状态和权限生成，而不是由 widget 在点击后临时拼装。

## 8. 视口显示规则

Viewport 中的坐标系、预览对象、highlight、collision overlay 等也属于视图。它们可以有临时 preview，但必须满足：

- 持久显示策略由 document/runtime 或明确的 task/session state 表达。
- Apply 后清理临时 preview，并由 document/runtime 重新驱动显示。
- Cancel 后恢复到进入任务前的状态。
- 多个视图都需要知道的显示状态，不应只存在于 viewport flag。

## 9. MainWindow 职责

MainWindow 是 shell/composer：

- 创建窗口、dock、menu、toolbar、controller。
- 连接顶层信号。
- 显示全局 status/error。
- 执行文件对话框等 shell 操作。

MainWindow 不应新增：

- 项目语义。
- 绑定和装配规则。
- 跨模块数据修补。
- 多 widget 手写刷新链。
- document 直接 mutation。

当 MainWindow 似乎需要知道某个语义时，优先把语义放入 document/service/controller/view model，再让 MainWindow 只负责连线。

## 10. 新功能实现模板

新增或修改 GUI 功能时，按以下顺序设计：

1. 定义真实状态属于 document、session、runtime 还是纯 UI。
2. 为真实状态补充 descriptor、service、command 或 task/session state。
3. 定义用户意图信号或 command 输入。
4. 通过 `mutateProject(...)` 或对应 service 修改状态。
5. 发布 typed event 或使用已有 document changed / selection changed / attachment changed 事件。
6. 让 module/controller 接收事件并重建 view model。
7. widget 只调用 `setDocumentView(...)` / `setViewModel(...)` 刷新。
8. 构建相关目标并尽可能添加非 GUI 测试覆盖 document/service 行为。

如果只能通过手动 GUI 测试验证，仍应先确保 document/service 层有可检查的不变量。

## 11. 与旧文档的关系

本契约优先约束后续 GUI、绑定、view model、document mutation、selection 和 viewport display 修改。

补充文档如 `docs/robotqtviewer_document_view_architecture.md`、`docs/robotqtviewer_document_view_module_contract.md` 记录实现结构与术语。若补充文档与本契约冲突，以本契约和当前源码为准。
