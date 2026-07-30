# Robot Mount 当前设计分析

## 目标

本文记录当前代码中机器人 mount 坐标系、tool/sensor attachment 数据结构，以及 GUI 设定方式。后续设计应以当前实现为基础，避免把已经存在的通用 mount 能力误判为两套独立的 `tool mount` / `sensor mount`。

## 已阅读的主要文件

- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectV3View.h`
- `SMRobotPlatform/SimulationProject/src/ProjectV3View.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectAttachmentEditor.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectIo.cpp`
- `SMRobotPlatform/SimulationRuntime/include/SimulationRuntime/ProjectRuntimeTypes.h`
- `SMRobotPlatform/SimulationRuntime/include/SimulationRuntime/RuntimeMountedAttachment.h`
- `SMRobotPlatform/SimulationRuntime/src/ProjectSimulationRuntime.cpp`
- `SMRobotPlatform/SimulationRuntime/src/RuntimeMountedAttachment.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupViewModel.h`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupViewModelBuilder.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupWidget.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupModuleController.cpp`
- `SMRobotApps/RobotViewerCore/ProjectRuntimeBuilder.cpp`
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`

## 当前核心数据结构

### `RobotMountDesc`

位置：`ProjectDocument.h`

当前字段为：

- `id`
- `name`
- `robotId`
- `linkName`
- `linkToMount`

这不是 sensor/tool 专用结构，而是“机器人某个 link 下的相对坐标系”。`linkToMount` 已经表达了用户期望的核心语义：mount 是基于 link 的一个相对位姿，可以用矩阵或 transform 表示。

### `AttachmentAssetDesc`

位置：`ProjectDocument.h`

当前字段包括：

- `assetKind`，例如 `tool` 或 `sensor`
- `assetType`
- `visualPath`
- `visualScale`
- `assetMountToVisual`
- `functionalFrames`
- `sensorIntrinsics`
- `visible`

tool/sensor 的区分主要在 asset 层，而不是 mount 层。sensor 的光学参数和功能 frame 也在 asset 层表达。

### `MountedAttachmentDesc`

位置：`ProjectDocument.h`

当前字段包括：

- `mountFrameId`
- `assetId`
- `mountToAssetMount`
- `visible`
- `enabled`

它表达“某个资产挂到某个 mount 上”，并保存 mount 到资产自身 mount frame 的偏移。

### `ProjectV3View`

位置：`ProjectV3View.h` / `ProjectV3View.cpp`

`ProjectV3View` 已经抽象出更通用的三层结构：

- `MountFrameView`
- `AssetView`
- `AttachmentView`

其中 `MountFrameView` 使用 `ownerType`、`ownerId`、`frameName`、`ownerFrameToMount` 表达“某个 owner 的某个 frame 上的 mount”。当前 `RobotMountDesc` 会映射为：

- `ownerType = "robot"`
- `ownerId = robotId`
- `frameName = linkName`
- `ownerFrameToMount = linkToMount`
- `sourceKind = "robotMount"`

这说明 v3 视图层已经接近目标模型，只是 `ProjectDocument` 内部仍保留 `robotMounts` 这个历史命名。

## 当前 GUI 设定方式

### `ToolSetup` 面板

位置：`SMRobotApps/RobotQtModules/ToolSetup`

当前 GUI 把 mount 编辑放在 `ToolSetup` 任务面板中，主要功能是：

- 选择当前 robot。
- 从 robot link 列表创建 mount。
- 删除未被 attachment 使用的 mount。
- 编辑 mount 所属 link。
- 编辑 `linkToMount`，即 `Link -> Mount` 位姿。
- 选择 mounted attachment。
- 编辑 `mountToAssetMount`，即 `Mount -> Asset mount` 位姿。
- 编辑 asset 的可视化路径、缩放、asset mount 到 visual 的位姿等。
- 控制 robot mount、tool mount、sensor optical/FOV 等 frame 显示。

### 当前偏向 tool 的地方

当前数据模型可以表达 sensor，但 GUI 有明显 tool 倾向：

- 面板命名为 `ToolSetup`。
- 文案包含 `Robot Mount / Flange`、`Tool Attachment`、`Tool mount`。
- `ToolSetupViewModelBuilder` 会过滤 `assetKind == "sensor"` 的 asset/attachment，因此主界面更像 tool 编辑器。
- 导入流程中有 `ensureRobotMountForToolImport()` 等 tool 命名函数。

这些是 UI/流程命名和过滤问题，不是底层 mount 必须区分 sensor/tool 的证据。

## 当前运行时与渲染方式

### 运行时 mount

位置：`SimulationRuntime`

`ProjectSimulationRuntime::loadRobotMounts()` 会从 `ProjectV3View` 的 `core.attachments.mountFrames` 载入 robot mount，并构建 `RuntimeRobotMount`。运行时更新时：

- 先获取 robot link 的世界位姿。
- 再乘以 `linkToMount` 得到 mount 世界位姿。

这符合“mount 是 link 上的相对坐标系”的设计。

### mounted attachment

`RuntimeMountedAttachmentGraph` 会根据 `AttachmentView` 连接 mount 和 asset。运行时保存：

- `linkToMount`
- `mountToAttachmentMount`
- `worldLink`
- `worldRobotMount`
- `worldAttachmentMount`
- `worldVisual`
- `worldTcp`

当前运行时已经可以在同一条路径下处理 tool/sensor，差异主要由 asset kind、functional frame、sensor intrinsics 决定。

### 视口显示

`ProjectScene` 会根据运行时 attachment 绘制 mount frame、asset frame、TCP frame 和 sensor FOV。虽然局部类型名仍有 `ToolAttachmentVisual` 等历史命名，但字段中已经包含 `assetKind` 与 sensor 相关数据。

## 当前 IO 与兼容性

`ProjectIo.cpp` 目前同时支持：

- 历史 flat 字段：`robotMounts`、`toolAssets`、`toolAttachments`、`tools`、`sensors`
- v3 视图字段：`core.attachments.mountFrames`、`assets`、`attachments`

v3 读取时会把 `MountFrameView` 转回 `RobotMountDesc`，写出时会从 `ProjectDocument` 构造 `ProjectV3View`。这提供了从旧命名迁移到通用 mount frame 的兼容通道。

## 基本功能总结

当前系统已经具备以下能力：

- 在 robot link 上定义一个相对 mount 坐标系。
- 将 tool 或 sensor asset 连接到 mount。
- 分别编辑 `linkToMount` 和 `mountToAssetMount`。
- 保存/读取 v3 JSON 中的通用 attachment 视图。
- 运行时根据 robot link 位姿更新 mount 和 attachment 世界位姿。
- 在视口中显示 mount、attachment、sensor FOV 等辅助可视化。

## 主要问题

- `ProjectDocument` 内部仍以 `robotMounts` 命名，概念上比 `MountFrame` 窄。
- GUI 仍以 tool setup 为主，sensor attachment 不在主列表中自然出现。
- mount 没有明确的 map/index facade，常见查询依赖 vector 扫描。
- `.sys.json` / `.rbt.json` 的系统包与机器人包边界尚未定义。
- 当前 IO 兼容了多种历史字段，但缺少一个面向未来的包格式策略。
- 部分运行时和渲染类型名仍带有 `Tool`，容易误导后续开发。

## 初步设计结论

后续应把“mount 是 owner frame 下的相对坐标系”作为核心概念。tool/sensor 不应体现在 mount 类型上，而应体现在 attachment asset 的能力、功能 frame 和参数上。

短期内不建议直接删除 `RobotMountDesc` 或历史字段。更安全的路线是先统一 GUI 语义和服务层访问，再让 `ProjectV3View` 的 `MountFrameView` 成为面向持久化和导入导出的稳定边界，最后再考虑是否把 `ProjectDocument` 内部命名迁移为更通用的 `MountFrameDesc`。
