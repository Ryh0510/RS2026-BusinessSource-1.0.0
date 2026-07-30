# RobotQtModules CollisionInspector 完全拆解计划

## 目标

将当前 `SMRobotApps/RobotQtModules/CollisionInspector` 从“统一碰撞检查器聚合模块”彻底拆解为多个可独立移动、独立构建、未来可独立窗口化的 Qt 模块。

目标状态：

- `CollisionLinkModelSetup`：连杆 Collision 模型设置模块。
- `CollisionDetectorConfig`：碰撞查询器/检测器配置模块。
- `CollisionRuntimeResults`：运行时碰撞检测结果显示模块。
- 原 `CollisionInspector` 不再作为业务聚合目录长期存在；如果需要保留，也只作为过渡兼容入口，最终应删除或改名为明确的临时 adapter。

## 非目标

- 不修改 `SMRobotCore`、`SMRobotPlatform` 或碰撞核心算法。
- 不改变项目文档 schema。
- 不在本计划中直接实现新功能。
- 不把三类模块继续隐藏在 `CollisionInspector` 子目录下。
- 不把当前问题简单处理为“删除目录”；必须先迁走它仍承担的聚合职责。

## 当前 `CollisionInspector` 仍在承担什么

当前根目录剩余文件说明它仍是一个聚合/编排层，而不是空壳：

- `CollisionInspectorPanel.*`
  - 当前统一创建并承载多个页面：
    - Link Models
    - Project Sets
    - Project Detectors
    - Project Results
    - Project Legacy
  - 它把三类模块的 widget 放在一个 tab panel 中。
  - 它仍暴露跨模块 API，例如 `setLinkModelsViewModel`、`setDetectors`、`setResults`、`setLegacyPairs`。

- `CollisionInspectorModuleController.*`
  - 当前统一持有三类模块 controller/facade：
    - `CollisionModelWorkbenchController`
    - `CollisionRequestWorkbenchController`
    - `CollisionDetectorWorkbenchController`
    - `CollisionLegacyPairWorkbenchController`
    - `CollisionResultViewController`
    - `CollisionSelectionSetViewController`
  - 它处理三类模块之间的刷新、事件发布、状态消息和 viewport reload。

- `CollisionInspectorRefreshCoordinator.*`
  - 当前统一刷新 link model、detector、selection set、legacy pair、runtime result。
  - 这是三类模块仍被耦合在一起的主要原因之一。

- `CollisionInspectorPanelSignalCoordinator.*`
  - 当前统一连接所有 panel 信号。
  - 信号内容混合了 selection set、detector、link model、proxy generation、export、legacy pair。

- `CollisionExport*`
  - 当前导出 sidecar/URDF 仍绑定在统一 Inspector 入口里。
  - 这部分更接近 link model setup 的输出动作，但也依赖 app services 和当前 UI 选择流程。

- `CollisionInspectorAppServices.h`
  - 当前作为三类碰撞 Qt 模块共同依赖的 app service interface。
  - 它不一定必须叫 Inspector，可以下沉或改名为更中性的 collision module services。

结论：现在不能直接删除 `CollisionInspector`，因为它仍然承担统一窗口、统一 controller、统一刷新、统一信号路由、统一导出的职责。

## 为什么可以完全拆解

可以完全拆解，但需要分阶段迁移职责：

1. 先让三个功能模块成为 `RobotQtModules` 顶层平行目录。
2. 再把 `CollisionInspectorPanel` 中的页面拆给三个模块各自的 widget/window。
3. 再把 `CollisionInspectorModuleController` 的方法和成员拆到三个模块自己的 controller。
4. 再把刷新协调、信号协调、导出动作迁到明确归属模块。
5. 最后删除或退役 `CollisionInspector` 聚合入口。

## 目标目录结构

```text
SMRobotApps/RobotQtModules/
  CollisionLinkModelSetup/
    CMakeLists.txt
    CollisionLinkModelsWidget.*
    CollisionLinkModelsController.*
    CollisionModelDocumentFacade.*
    CollisionModelWorkbenchController.*
    CollisionRequestDocumentFacade.*
    CollisionRequestWorkbenchController.*
    CollisionExportDocumentFacade.*
    CollisionExportWorkbenchController.*
    CollisionQualityMessageState.*

  CollisionDetectorConfig/
    CMakeLists.txt
    CollisionDetectorsWidget.*
    CollisionDetectorsController.*
    CollisionDetectorWorkbenchController.*
    CollisionDetectorWorkbenchDocumentFacade.*
    CollisionSelectionSetsWidget.*
    CollisionSelectionSetViewController.*
    CollisionSelectionSetWorkbenchController.*
    CollisionLegacyPair*

  CollisionRuntimeResults/
    CMakeLists.txt
    CollisionResultsWidget.*
    CollisionResultsController.*
    CollisionResultDocumentFacade.*
    CollisionResultViewController.*

  CollisionShared/
    CMakeLists.txt
    CollisionModuleAppServices.h
    CollisionDocumentEventPublisher.*
    common collision UI DTOs if still shared

  CollisionInspector/
    optional temporary compatibility adapter only
```

## 分阶段计划

### 阶段 1：收口当前半迁移状态

目标：
- 保证当前工作区重新构建通过。
- 保留已经拆出的三个顶层模块目录。
- 不再继续扩大行为变化。

需要做：
- 修复当前 `RobotQtModulesCollisionInspector` 因 `unique_ptr` 不完整类型导致的编译错误。
- 确认 `RobotQtViewer` 可以链接新的三个 target。
- 记录当前 `CollisionInspector` 仍是兼容聚合入口。

验证：

```bat
cmake --build build --config Release --target RobotQtViewer
```

### 阶段 2：拆掉统一 Panel

目标：
- `CollisionInspectorPanel` 不再同时拥有三类页面。
- 三个模块各自暴露自己的 widget 或 panel。

建议：
- `CollisionLinkModelSetupWidget` 或沿用 `CollisionLinkModelsWidget` 作为该模块顶层 widget。
- `CollisionDetectorConfigWidget` 聚合 detector、selection set、legacy pair。
- `CollisionRuntimeResultsWidget` 或沿用 `CollisionResultsWidget` 作为结果显示顶层 widget。

需要移动/改造：
- 将 `CollisionInspectorPanel` 中创建 tab 的逻辑拆到三个模块内部。
- 如果仍需要旧的统一碰撞页面，可新建临时 `CollisionInspectorCompatibilityPanel`，只组合三个模块 widget，不再承载业务 API。

停止条件：
- 如果需要改 `MainWindow` 对 workbench 的窗口管理方式，先停止并单独计划。

### 阶段 3：拆 `CollisionInspectorModuleController`

目标：
- 三类模块各自拥有 controller。
- 不再由一个 mega controller 同时持有所有子模块 facade/controller。

建议拆分：
- `CollisionLinkModelSetupModuleController`
  - 管理 link collision override、proxy generation、variant selection、sidecar/URDF export。
- `CollisionDetectorConfigModuleController`
  - 管理 detector、selection set、legacy robot-object pair、active detector context。
- `CollisionRuntimeResultsModuleController`
  - 管理 runtime result refresh、contact/nearest display、selection dependent result updates。

需要处理：
- 统一状态消息可以通过已有 `RobotQtViewerEventHub` 或一个 shared app service 发出。
- viewport reload 不应由三个模块各自随意调用，建议封装到 shared service。

### 阶段 4：拆刷新和信号协调器

目标：
- 删除或退役：
  - `CollisionInspectorRefreshCoordinator`
  - `CollisionInspectorPanelSignalCoordinator`

替代：
- 每个模块 controller 只连接自己的 widget 信号。
- 跨模块刷新通过事件 hub 或明确的 shared collision event 完成。

示例：
- Link model 更新后发布 `collisionModelChanged(sourceId)`。
- Detector config 监听并刷新 detector detail。
- Runtime results 监听 runtime/collision changed 后刷新结果表。

### 阶段 5：处理 Shared service 命名

目标：
- `CollisionInspectorAppServices` 改为更中性的名称，例如：
  - `CollisionModuleAppServices`
  - `CollisionWorkbenchServices`

原因：
- 如果三个模块是独立窗口/独立 workbench，它们不应依赖名为 Inspector 的服务接口。

注意：
- 这是 public include 名称变化，建议单独提交。

### 阶段 6：移除 `CollisionInspector`

删除条件：
- `RobotQtViewer` 不再直接创建 `CollisionInspectorPanel`。
- `RobotQtViewer` 不再直接依赖 `RobotQtModulesCollisionInspector`。
- 三个新模块都能独立构建并被 app shell 分别挂载。
- 旧统一入口没有兼容需求。

可选结果：
- 删除 `CollisionInspector/`。
- 或保留为 `CollisionWorkbenchLegacyAdapter/`，但不得再承载业务逻辑。

## 风险

- 当前统一 `CollisionInspectorModuleController` 内部方法很多，直接一次性拆完容易引入行为回归。
- `CollisionInspectorPanel` 当前承担 UI 聚合和接口转发，拆它会影响 workbench 显示方式。
- 导出 sidecar/URDF 逻辑归属需要人工确认：更偏 LinkModelSetup，但可能仍需要 app shell 文件选择能力。
- 如果用户希望三个模块作为独立窗口，还需要检查 `RobotQtViewerWorkbench` 当前是否支持多个 collision workbench 入口。

## 建议提交顺序

1. 提交当前 CMake/目录拆分收口，保证可构建。
2. 单独提交 `CollisionInspectorPanel` 拆分。
3. 单独提交三个 module controller 拆分。
4. 单独提交 service 重命名。
5. 单独提交删除或退役 `CollisionInspector`。

## 推荐下一步

先执行阶段 1：修复当前半迁移状态并构建通过。

然后由人工确认：
- 三个模块是否都要在 UI 上成为独立 dock/window/workbench。
- 是否保留一个临时“Collision”总入口。
- 导出 sidecar/URDF 属于 `CollisionLinkModelSetup` 还是 app shell 全局命令。
