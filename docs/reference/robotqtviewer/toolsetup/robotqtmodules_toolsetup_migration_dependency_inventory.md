# ToolSetup 平级迁移依赖清单

## 结论

`ToolSetup` 可以作为第一批平级迁移试点，但它还不是完全独立的可复用 Qt module。

本轮可以先完成：

- 物理目录迁移到 `SMRobotApps/RobotQtModules/ToolSetup`
- target 名升级为 `RobotQtModulesToolSetup`
- 保留 `RobotQtViewerToolSetup` alias 作为兼容入口
- 移除对 `Modules/CollisionInspector` 的残留 include 目录
- 将 `RobotQtViewerAppController.h` 对 `CollisionWorkflowController.h` 的直接 include 改为 cpp 内部依赖
- 将 `CoreWidgets` 迁移到 `SMRobotApps/RobotQtModules/CoreWidgets`
- 让 `ToolSetup` 链接 `RobotQtModulesCoreWidgets`
- 将仍需保留的 dialog facade 迁移到 `SMRobotApps/RobotQtModules/Dialogs`
- 已移除 `ToolSetup` 对 `RobotQtModulesDialogs` 的链接；ToolSetup 不再通过旧 dialog fallback 编辑 attachment 或 tool asset。
- 新增 `ToolSetupAppServices`，由 ToolSetup 依赖接口而不是具体 viewer app controller。
- 新增 `RobotQtViewerToolSetupAppServicesAdapter`，在 viewer shell 侧适配 `RobotQtViewerAppController`。
- 将 `ToolAttachmentCommandController` 迁移到 `RobotQtModules/ToolSetup`。
- 将 `RobotQtModulesToolSetup` target 定义移动到 `RobotQtModules/CMakeLists.txt`。
- 新增 ToolSetup asset library view model / task panel 选择器，未挂载 tool asset 可直接在 ToolSetup 中编辑。
- 新增 robot mount transform、link 切换、mount 新增和空 mount 删除 task panel 入口；已有 robot mount 的 `linkToMount` / `linkName` 可通过 ToolSetup `Apply Changes` 更新，mount 新增/删除通过 ToolSetup action 写入 document。
- SceneExplorer 的 `ConfigureRobotFlange` action 已改为进入 ToolSetup task panel 并聚焦 robot mount 编辑，不再打开 `RobotFlangeConfigDialog`。

## 已确认的内部组成

源码：

- `ToolSetupModuleController.cpp`
- `ToolSetupModuleController.h`
- `ToolSetupViewModelBuilder.cpp`
- `ToolSetupViewModelBuilder.h`
- `ToolSetupWidget.cpp`
- `ToolSetupWidget.h`
- `ToolSetupViewModel.h`

职责：

- `ToolSetupWidget`：显示工具装配 view model，发出用户 intent。
- `ToolSetupViewModelBuilder`：从 `ProjectDocument` 和选择上下文构建显示模型。
- `ToolSetupModuleController`：连接 Widget intent、DocumentContext、ToolSetupAppServices 和 ProjectDocumentService。

## 已隔离 RobotQtViewer shell 的内容

### ToolSetupAppServices / RobotQtViewerAppController

位置：

- `RobotQtModules/ToolSetup/ToolSetupAppServices.h`
- `RobotQtViewer/RobotQtViewerToolSetupAppServicesAdapter.*`

用途：

- 请求 viewport reload。
- 协调 selected robot/link。
- 写入 Inspector 上下文。

当前处理：

- ToolSetup 只依赖 `ToolSetupAppServices`。
- viewer shell 通过 `RobotQtViewerToolSetupAppServicesAdapter` 把现有 `RobotQtViewerAppController` 行为注入 ToolSetup。
- `ToolSetupModuleController` 不再 include 或持有 `RobotQtViewerAppController`。

后续建议：

- 如果后续多个模块复用相同 app service，可再评估是否抽为更通用的 module app service。
- 目前保持 ToolSetup 专用接口，避免过早扩大抽象面。

### ToolAttachmentCommandController

位置：

- `RobotQtModules/ToolSetup/ToolAttachmentCommandController.*`

用途：

- 调用 `simulation_project::ProjectAttachmentCommands` 应用 mounted attachment 更新。
- 调用 `simulation_project::ProjectAttachmentCommands` 应用 attachment asset 更新。

当前处理：

- 已从 `RobotQtViewer` 根目录迁移到 `RobotQtModules/ToolSetup`。
- `RobotQtViewerAppController` 已移除 ToolSetup 专用的 `ToolWorkflowController` 成员。
- ToolSetup target 不再链接 `RobotQtViewerControllers`。

### RobotQtViewerDialogController

位置：

- `ToolSetupModuleController.cpp`

用途：

- 编辑 mounted attachment。
- 编辑 tool asset。

当前处理：

- 历史上曾迁移到 `RobotQtModules/Dialogs` 作为过渡 facade。
- 当前 `RobotQtViewerDialogController`、`RobotQtModulesDialogs` 和旧 Dialogs 源码已删除。
- ToolSetup 已不再 include `RobotQtViewerDialogController.h`，也不再链接 `RobotQtModulesDialogs`。

后续建议：

- 不恢复 ToolSetup dialog fallback；新增编辑入口优先进入 ToolSetup task panel。

### RobotViewport / ProjectScene

位置：

- `ToolSetupModuleController.cpp`

用途：

- 控制 tool frame visibility。
- 访问 viewport 相关显示状态。

当前处理：

- 已新增 `RobotQtViewerViewportServices` shared 接口。
- 已新增 viewer shell 侧 `RobotQtViewerViewportServicesAdapter` 适配 `RobotViewport`。
- ToolSetup 已改为通过 `RobotQtViewerDocumentContext::viewportServices()` 执行 viewport 选择和 frame visibility 操作。
- ToolSetup 不再直接 include `RobotViewport.h` 或使用 `ProjectScene::ToolFrameVisibility`。

后续建议：

- 继续评估是否把部分 viewport 操作改为 `RobotQtViewerEventHub` 事件，由 shell/viewport 订阅处理。

## 仍依赖 RobotQtViewer 根目录通用 Widget 的内容

### RobotQtWidgetUtils

位置：

- `ToolSetupWidget.cpp`

当前处理：

- 已迁移到 `RobotQtModules/CoreWidgets`。
- `ToolSetup` 通过 `RobotQtModulesCoreWidgets` 获得 include 和链接关系。

### ToolAssetEditorWidget / ToolTransformEditorWidget

位置：

- `ToolSetupWidget.cpp`
- `ToolSetupWidget.h`

当前处理：

- 已迁移到 `RobotQtModules/CoreWidgets`。
- ToolSetup target 链接 `RobotQtModulesCoreWidgets`。

### 旧 dialog edit model

位置：

- 旧 `RobotQtViewerEditModels.h`

当前处理：

- ToolSetup 不再依赖 viewer 根目录 edit model 头文件。
- 旧 edit model 已随 base/flange/tool asset dialog 清理删除。

后续建议：

- 后续新增 UI 输入形状应优先放在对应模块 view model / task panel model 中，而不是恢复通用 dialog edit model。

## CollisionInspector 依赖检查

当前 `RobotQtViewerToolSetup` 在 CMake 中 private include 了：

```text
${CMAKE_CURRENT_SOURCE_DIR}/Modules/CollisionInspector
```

代码检索未发现 ToolSetup 源码实际包含 CollisionInspector 类型。

结论：

- 这是残留 include 目录，可以移除。
- ToolSetup 不应直接依赖 CollisionInspector。

## 本轮迁移结果

当前已经完成：

- 源码移动到 `SMRobotApps/RobotQtModules/ToolSetup`
- target 改为 `RobotQtModulesToolSetup`
- CMake target 已移动到 `RobotQtModules/CMakeLists.txt`
- `CoreWidgets` 已移动到 `SMRobotApps/RobotQtModules/CoreWidgets`
- 旧 `Dialogs` 过渡模块已删除
- `ToolSetupAppServices` 已切断 ToolSetup 对 `RobotQtViewerAppController` 的直接依赖
- `ToolAttachmentCommandController` 已归入 ToolSetup 模块

仍保留的过渡点：

- ToolSetup 已不再调用 `RobotQtViewerDialogController` facade；未挂载 tool asset 已通过 asset library view model 进入 task panel 编辑，不需要恢复旧 dialog fallback。
- Dialogs 已删除 base/flange/tool asset/collision detector 旧 dialog、preview surface 和 facade。

后续建议：

- 继续减少 `MainWindow` 对 `ProjectDocument` 的直接读取和长期 workflow 编排。
