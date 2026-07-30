# Robot Mount 通用坐标系与系统包修改计划

## 目标

- 将 mount 明确定义为“某个 owner frame 下的相对坐标系”，当前阶段重点是 robot link 下的相对坐标系。
- 消除 GUI 和代码路径中把 mount 误解为 `tool mount` 或 `sensor mount` 的设计倾向。
- 保持 tool/sensor 差异在 attachment asset 层表达，而不是在 mount 层表达。
- 设计 `.sys.json` 与 `.rbt.json` 的保存边界：
  - `.sys.json` 表达完整系统。
  - `.rbt.json` 表达可复用的机器人实体包。
- 保留现有 v3 JSON 和历史字段兼容性。

## 非目标

- 本次不直接删除历史字段 `robotMounts`。
- 本次不直接重命名公开 C++ API。
- 本次不一次性改完整个 ProjectDocument 持久化 schema。
- 本次不引入新第三方依赖。
- 本次不改变 robot、collision、runtime、render 模块依赖方向。

## 当前已确认事实

- 当前没有独立的 `SensorMountDesc` 或 `ToolMountDesc` 持久化结构。
- `RobotMountDesc` 已经表达了 link 相对 mount frame。
- `AttachmentAssetDesc::assetKind` 保存 tool/sensor 差异。
- `MountedAttachmentDesc::mountToAssetMount` 保存 mount 到资产 mount frame 的偏移。
- `ProjectV3View` 已有 `MountFrameView`、`AssetView`、`AttachmentView` 三层通用视图。
- `ProjectIo.cpp` 已能读写 `core.attachments.mountFrames/assets/attachments`。
- `ToolSetup` GUI 当前会过滤 sensor asset，并使用大量 tool/flange 文案。

## 需要继续检查的文件

- `SMRobotApps/RobotQtModules/ToolSetup/*`
- `SMRobotApps/RobotQtModules/SceneExplorer/*`
- `SMRobotApps/RobotQtViewer/src/MainWindow*.cpp`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectV3View.h`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectAttachmentEditor.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectIo.cpp`
- `SMRobotPlatform/SimulationRuntime/*`
- `SMRobotApps/RobotViewerCore/*`
- 现有 regression 和 smoke test。

## 预期修改文件

第一阶段预期只修改 GUI 和文档：

- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupWidget.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupViewModelBuilder.cpp`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupModuleController.cpp`
- `docs/robot_mount_current_design_analysis.md`
- 本计划文件。

第二阶段以后才考虑修改：

- `ProjectDocument.h`
- `ProjectV3View.h`
- `ProjectV3View.cpp`
- `ProjectDocumentService.cpp`
- `ProjectAttachmentEditor.cpp`
- `ProjectIo.cpp`
- `SimulationRuntime` mount/attachment runtime 文件。
- `RobotViewerCore` runtime builder 和 scene 文件。
- regression/smoke test。

## 目标数据模型

### Mount

语义：

- mount 是一个命名坐标系。
- mount 依附于某个 owner 的某个 frame。
- 对机器人而言，owner 是 robot，frame 是 link。
- mount 自身不区分 tool 或 sensor。

建议字段：

- `id`
- `name`
- `ownerType`
- `ownerId`
- `frameName`
- `ownerFrameToMount`
- `metadata`

当前可先用 `ProjectV3View::MountFrameView` 作为外部稳定视图，不急于替换 `ProjectDocument::robotMounts`。

### Attachment Asset

语义：

- asset 表示可挂载实体。
- tool/sensor/camera/spray gun 等能力由 asset kind、functional frames 和参数描述。
- asset 不应决定 mount 的类型。

建议保留字段：

- `assetKind`
- `assetType`
- `visualPath`
- `visualScale`
- `assetMountToVisual`
- `functionalFrames`
- `sensorIntrinsics`
- `metadata`

### Mounted Attachment

语义：

- mounted attachment 表示一个 asset 被挂到一个 mount 上。
- `mountToAssetMount` 表达安装偏移。

建议保留字段：

- `mountFrameId`
- `assetId`
- `mountToAssetMount`
- `enabled`
- `visible`
- `metadata`

## 保存位置设计

### `.sys.json`

完整系统文件应保存：

- robots
- mount frames
- robot link collision models 或 collision overrides
- attachment assets
- mounted attachments
- sensors/tools 的 asset 参数与 functional frames
- objects
- point clouds
- collision query config
- view/session 相关非核心显示信息

建议让 `.sys.json` 复用当前 v3 的 `core` 分层思路，并把 `core.attachments.mountFrames/assets/attachments` 作为 mount/asset/attachment 的规范位置。

### `.rbt.json`

机器人包文件应保存一个可复用机器人实体：

- robot 描述或 robot 资源引用。
- 属于该 robot 的 mount frames。
- 属于该 robot link 的 collision overrides。
- 可随 robot 复用的 attachment assets。
- robot-local mounted attachments。
- robot-local sensor/tool 配置。

`.rbt.json` 不应保存完整系统中的其他 robot、object、point cloud、全局视图布局等内容。全局 collision query 规则需要区分：

- robot 自身默认 collision 配置可以进入 `.rbt.json`。
- 多机器人、多 object 的系统级 collision 配置留在 `.sys.json`。

## Map 与索引策略

用户期望“用 map 保存，便于标识和获取”。建议实现上区分两层：

- JSON 持久化继续使用 array，便于保持顺序、稳定 diff 和兼容现有格式。
- 服务层和运行时构建 `id -> mount`、`(ownerId, frameName) -> mount ids` 等索引，提供 map 语义访问。

这样可以同时获得可读 JSON 和高效查询。

## 分阶段执行计划

### 阶段 0：现状分析与设计记录

已执行：

- 阅读当前 mount、attachment、ToolSetup、ProjectIo、runtime、render 相关代码。
- 写入 `docs/robot_mount_current_design_analysis.md`。
- 写入本计划文件。

### 阶段 1：GUI 概念统一

目标：

- 把 GUI 文案从 `Tool mount`、`Robot Mount / Flange` 调整为更通用的 `Mount Frame`、`Attachment`。
- 让 ToolSetup 面板不再暗示 mount 只服务 tool。
- 明确区分 `Link -> Mount` 和 `Mount -> Attachment` 两段 transform。

预计修改：

- `ToolSetupWidget.cpp`
- `ToolSetupViewModelBuilder.cpp`
- `ToolSetupModuleController.cpp`

行为预期：

- 点击 robot link/mount 时，右侧仍可编辑 mount frame。
- 点击 mounted attachment 时，右侧可编辑 attachment offset。
- 现有 tool 导入行为保持不变。
- 不改变持久化格式。

### 阶段 2：Service 层 mount 索引与通用命名

目标：

- 添加不破坏兼容性的查询 facade，例如按 id、robot/link 查找 mount。
- 减少 GUI 和 runtime 中重复 vector 扫描。
- 对外暴露通用 mount frame 语义，内部仍可暂时使用 `RobotMountDesc`。

预计修改：

- `ProjectDocumentService.cpp`
- `ProjectAttachmentEditor.cpp`
- 相关头文件。

行为预期：

- 现有 `addRobotMount`、`updateRobotMountTransform` 等 API 保留。
- 新代码优先通过通用 mount frame 查询接口访问。

### 阶段 3：Attachment GUI 支持 sensor/tool 统一展示

目标：

- 不再在主编辑路径中过滤 sensor asset。
- GUI 以 attachment asset 的 kind/type 显示 tool/sensor。
- sensor 的 optical frame/FOV 作为 asset 能力显示，而不是 mount 类型。

预计修改：

- `ToolSetupViewModelBuilder.cpp`
- `ToolSetupViewModel.h`
- `ToolSetupWidget.cpp`
- `ToolSetupModuleController.cpp`
- SceneExplorer 相关 view model builder。

行为预期：

- 点击 tool attachment 和 sensor attachment 都能进入同一套 mount/attachment 编辑逻辑。
- sensor 专有参数仍只在 sensor asset 有效时显示。

### 阶段 4：`.sys.json` 格式入口

目标：

- 把完整系统文件扩展名定义为 `.sys.json`。
- 继续读取旧 `.v3.json`。
- 写出时使用当前 v3 core 分层，明确 `core.attachments` 是 mount/asset/attachment 的规范位置。

预计修改：

- `ProjectIo.cpp`
- 文件对话框和项目打开/保存入口。
- smoke/regression test。

行为预期：

- 旧 v3 文件可读。
- 新保存入口可生成 `.sys.json`。
- 不破坏现有工程。

### 阶段 5：`.rbt.json` 机器人包导出/导入

目标：

- 支持导出单个 robot 及其 robot-local 配置。
- 支持在其他 system 中引用或导入该 robot package。

包内容：

- robot 资源引用。
- robot-local mount frames。
- robot-local collision overrides。
- robot-local attachment assets。
- robot-local mounted attachments。
- robot-local tool/sensor 参数。

预计修改：

- `SimulationProject` 新增 robot package view/serializer。
- `ProjectDocumentService` 新增导入/导出接口。
- RobotQtViewer 文件菜单或相关 task panel 新增导入/导出动作。
- regression/smoke test。

行为预期：

- 可以从一个 system 导出 `.rbt.json`。
- 可以在另一个 system 中导入并自动重建 mount/attachment/collision 局部配置。

## 验证方法

每个阶段至少执行：

- `cmake --build build --config Debug --target RobotQtViewer`
- 相关 smoke/regression target。
- `git diff --check`

阶段 4/5 额外验证：

- 旧 v3 JSON 可读。
- 新 `.sys.json` 可写可读。
- `.rbt.json` 导出后再导入，mount、collision、tool/sensor attachment 均保持。

## 为什么这是最小计划

- 先统一用户可见概念，不立即破坏存量 schema。
- 复用现有 `ProjectV3View` 的通用 attachment 结构。
- 用服务层索引提供 map 语义，不把 JSON 直接改成 object map。
- `.sys.json` 与 `.rbt.json` 分开实施，避免一次修改同时影响项目 IO、GUI、runtime、render 和测试。

## 停止条件

出现以下情况应停止并重新确认：

- 需要删除或重命名现有公开 API。
- 需要一次修改超过 12 个 source/config 文件。
- 需要改变模块依赖方向。
- 需要引入新依赖。
- 旧 v3 文件兼容性无法保持。
- `.rbt.json` 与 `.sys.json` 边界无法通过现有数据结构表达。
- 两次构建失败且失败原因不同。

## 当前执行状态

已按本计划执行到阶段 5 的最小闭环：

- 阶段 0 已完成：现状分析写入 `docs/robot_mount_current_design_analysis.md`。
- 阶段 1 已完成：`ToolSetup` 的用户可见文案已统一到 `Mount Frame` / `Attachment` / `Attachment Asset`。
- 阶段 2 已完成：`ProjectDocumentService` 增加了按 robot、robot/link 查询 mount frame 的只增不改接口。
- 阶段 3 已完成：`ToolSetupViewModelBuilder` 和控制器不再把 `assetKind == "sensor"` 从主 attachment 编辑路径中过滤掉。
- 阶段 4 已完成：RobotQtViewer 打开/保存入口已接受并优先显示 `.sys.json`，保存仍复用当前 v3 core 分层 writer。
- 阶段 5 已完成最小闭环：`ProjectIo` 支持导出/导入 `.rbt.json` robot package，并在 File 菜单提供导入/导出入口。

当前 `.rbt.json` 导入策略是保守的：如果 robot、mount frame、attachment asset 或 mounted attachment 的 id 与当前系统冲突，则导入失败并提示错误。自动重命名和引用重写应作为后续单独任务实现。
