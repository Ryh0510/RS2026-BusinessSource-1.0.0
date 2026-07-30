# SimWorkbench Common 边界说明

生成时间：2026-07-09

## 1. 目标

阶段 C 的目标是先建立 `SMRobotWorkbenchCommon` 这个独立 Workbench common package，并明确公共 Qt workbench SDK、app shell、业务 Workbench 三者的边界。

本阶段不迁移真实业务源码，不改变 `RobotQtViewer` 的运行行为，也不引入 runtime DLL plugin loader。

## 2. 当前落地形态

新增逻辑 package：

```text
SimWorkbench/SMRobotWorkbenchCommon
```

新增 component：

```text
WorkbenchCommon
```

安装后外部 consumer 使用方式：

```cmake
find_package(SMRobotWorkbenchCommon CONFIG REQUIRED COMPONENTS WorkbenchCommon)
target_link_libraries(app PRIVATE SMRobotWorkbenchCommon::WorkbenchCommon)
```

当前 `WorkbenchCommon` 仍是 `INTERFACE` shell。源码构建时，它通过 build interface 聚合现有：

```text
RobotQtModulesShared
RobotQtModulesCoreWidgets
```

安装后，它只暴露稳定公共依赖：

```text
Qt5::Widgets
```

当前 common component 还没有发布公共头文件，因此不把源码构建所需的 `Common::Utility`、`SMRobotPlatform::SimulationProject`、`SMRobotPlatform::SimulationRuntime` 外泄给安装后的 consumer。后续若 `WorkbenchCommon` 发布的头文件真实包含这些类型，再按头文件依赖补回公开依赖。

## 3. 归属规则

### 可以进入 Workbench common

满足以下条件的内容可以进入 `SMRobotWorkbenchCommon`：

- 所有 Workbench 都可能需要。
- 不拥有具体业务模式语义。
- 不直接表达 collision、tool、spray、motion planning、digital twin 的领域规则。
- 只提供注册、事件、文档上下文、基础 viewport 服务、通用控件或 app-to-workbench 接口。

短期候选：

```text
RobotQtViewerWorkbench
RobotQtViewerWorkbenchPackageRegistry
RobotQtViewerWindowConfig
RobotQtViewerEventHub
RobotQtViewerEvents
RobotQtViewerDocumentViewRegistry
RobotQtViewerDocumentContext
RobotQtViewerDocumentController
RobotQtViewerSelectionModel
RobotQtViewerViewportPreviewState
RobotQtViewerViewportServices
InspectorContext
RobotQtWidgetUtils
```

### 暂留 app shell

以下内容属于 `RobotQtViewer` 或 `SMRobotApps` 的 composition root，不应搬进业务 Workbench：

```text
MainWindow
RobotQtViewer executable target
顶层菜单、dock、应用生命周期、Qt runtime deploy
工作模式装配顺序
用户配置文件与窗口持久化入口
```

这些内容可以调用 common 和业务 Workbench，但不应拥有业务模式语义。

### 随业务 Workbench 迁移

以下内容含有明确业务语义，不能因为位于 `Shared` 或 `CoreWidgets` 就直接放入 common：

```text
CollisionDetectorCommandController
CollisionDetectorDocumentFacade
CollisionDetectorsViewModel
CollisionLinkModelDocumentCommandController
CollisionLinkModelDocumentFacade
CollisionLinkModelVariantCommandController
CollisionPairWorkflowController
CollisionRuntimeViewModel
CollisionSelectionSetDocumentFacade
CollisionSelectionSetsController
CollisionSelectionSetsViewModel
SceneEntityWorkflowController
ProjectSessionWorkflowController
ViewportReloadWorkflowController
ToolAssetEditorWidget
ToolTransformEditorWidget
```

其中 `ToolAssetEditorWidget`、`ToolTransformEditorWidget` 虽然是控件，但直接服务 tool / attachment 编辑，优先随 `SMRobotWorkbenchProjectAssembly` 迁移。只有后续证明它们成为跨业务通用编辑器时，才考虑再提升到 common。

## 4. 当前依赖方向

目标依赖方向：

```text
RobotQtViewer
    -> SMRobotWorkbenchCommon::WorkbenchCommon
    -> SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench
    -> SMRobotWorkbenchCollisionConfig::CollisionConfigWorkbench
    -> ...
```

业务包只能依赖 common，不能反向让 common 依赖具体业务包。

领域语义仍归属：

```text
SMRobotCore
SMRobotPlatform
SMRobotSpray
未来明确的非 UI 业务 SDK
```

## 5. 下一阶段迁移约束

阶段 D 迁移 `ProjectAssemblyWorkbench` 时，应先迁移：

```text
SceneExplorer
ToolSetup
```

但迁移前需要逐个拆分它们对 `RobotQtModulesShared` / `CoreWidgets` 的引用：

- 对 common 接口的引用保留为 `SMRobotWorkbenchCommon::WorkbenchCommon`。
- 对 tool / scene 业务 controller 的引用随 `ProjectAssemblyWorkbench` 移动。
- 如果发现某个 shared 类同时服务 collision 和 project assembly，需要先判断它是公共接口还是缺少更小的业务服务抽象。

## 6. 完成标准

阶段 C 完成时应满足：

- `SMRobotWorkbenchCommon` 能源码构建。
- `SMRobotWorkbenchCommon` 能 install。
- 外部 consumer 能独立 `find_package(SMRobotWorkbenchCommon)`。
- 其他 Workbench shell 不再直接暴露 `RobotQtModulesShared` / `RobotQtModulesCoreWidgets`，而是依赖 `SMRobotWorkbenchCommon::WorkbenchCommon`。
- `RobotQtViewer` 构建和 profile 启动不退化。

## 7. 阶段 D 后的状态

阶段 D 已把 `SceneExplorer` 与 `ToolSetup` 迁移到：

```text
SimWorkbench/SMRobotWorkbenchProjectAssembly/ProjectAssemblyWorkbench
```

`RobotQtViewer` 现在通过 `SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench` 获得这两个模块，而不是直接链接它们。

当前仍保留的限制：

- `RobotQtModulesShared` 与 `RobotQtModulesCoreWidgets` 仍位于 `SMRobotApps/RobotQtModules`。
- `ProjectAssemblyWorkbench` 的安装形态仍是 interface shell。
- `SceneExplorer` / `ToolSetup` 的真实库 target 尚未作为 installed component 暴露给外部 consumer。

## 8. 阶段 D2/E 后的状态

`SMRobotWorkbenchCommon` 现在不再只是空壳 interface：

- `WorkbenchCommon` 仍是上层统一入口。
- `RobotQtModulesShared` 与 `RobotQtModulesCoreWidgets` 作为真实安装组件归属 `SMRobotWorkbenchCommon`。
- 源码目录短期仍位于 `SMRobotApps/RobotQtModules`，但安装包归属已经转移到 common。

`SMRobotWorkbenchProjectAssembly` 当前真实组件：

- `ProjectAssemblyWorkbench`
- `ProjectAssemblySceneExplorer`
- `ProjectAssemblyToolSetup`

`SMRobotWorkbenchCollisionConfig` 当前真实组件：

- `CollisionConfigWorkbench`
- `CollisionDetectorConfig`
- `CollisionLinkModelSetup`
- `CollisionRuntimeResults`

边界判断：

- `CollisionRuntimeResults` 当前属于 collision 领域 UI 结果视图，不进入 common。
- `RobotRunWorkbench` 可以复用该组件，但不拥有或复制它。
- 如果未来运行结果、规划结果、碰撞结果需要统一展示，再抽一个 result display 包，而不是把 collision 结果提前放进 common。

当前限制：

- 真实组件的安装头文件仍是迁移模块现有头文件，不是稳定 SDK API 合约。
- 外部 consumer 会触发较宽的 Core/Platform 第三方依赖解析，下一阶段需要统一 prebuilt 第三方依赖入口。
