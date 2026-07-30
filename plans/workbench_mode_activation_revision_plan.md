# 工具栏模式点亮与初始面板宽度修订计划

## 目标

- 初始打开 `RobotQtViewer` 时，将左侧场景树面板和右侧任务面板的默认宽度扩大约 25%。
- 将工具栏 Modes 中尚未启用的四个模式点亮：`Motion Planning`、`Spray Process`、`Coating Analysis`、`Digital Twin`。
- 为这四个模式生成并接入仓库内 ribbon PNG 图标。
- 当前阶段仅完成模式切换占位：点击后进入对应 workbench，右侧显示状态面板，状态栏显示当前模式；不实现具体业务操作。

## 非目标

- 不新增喷涂、涂层分析、数字孪生或规划重算的底层业务语义。
- 不连接真实机器人控制器，不新增外部设备通信、认证、遥测或安全联锁逻辑。
- 不修改项目文件 schema、保存/加载语义、运行时 collision/trajectory 语义。
- 不重命名现有内部 `TrajectoryPlanning` 枚举，用户可见名称改为 `Motion Planning`。

## 现状与不变量

- `Project Assembly`、`Collision Config`、`Robot Run` 已经通过 `RobotQtViewerWorkbenchManager` 切换 GUI 会话 mode。
- `TrajectoryPlanning`、`SprayProcess`、`CoatingAnalysis` 描述符已存在，但 QAction 被禁用，且未连接触发逻辑。
- `DigitalTwin` 尚无 workbench 枚举、描述符、QAction 和工具栏入口。
- 本轮修改属于 GUI 会话层：mode 切换只改变当前 workbench descriptor、tree projection/right panel 选择和 viewport interaction mode，不拥有底层项目事实。

## 拟修改文件

- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerWorkbench.h`
- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerWorkbench.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerRibbonModel.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.h`
- `SMRobotApps/RobotQtViewer/RobotQtViewerToolbarController.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerResources.qrc`
- `SMRobotApps/RobotQtViewer/RobotQtViewerLanguage.cpp`
- `SMRobotApps/RobotQtViewer/MainWindow.h`
- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `SMRobotApps/RobotQtViewer/resources/icons/ribbon/motion_planning_workbench.png`
- `SMRobotApps/RobotQtViewer/resources/icons/ribbon/spray_process_workbench.png`
- `SMRobotApps/RobotQtViewer/resources/icons/ribbon/coating_analysis_workbench.png`
- `SMRobotApps/RobotQtViewer/resources/icons/ribbon/digital_twin_workbench.png`

## 设计说明

- 面板宽度是窗口布局的 UI 会话状态，拥有层为 `MainWindow::createPanels()` 的 dock 初始化代码；将 `resizeDocks({ robotDock, resultDock }, { 375, 450 }, Qt::Horizontal)` 调整为约 25% 放大后的 `{ 469, 563 }`。
- 后四个模式使用现有 `RobotQtViewerWorkbenchManager::enterWorkbench(...)` 统一切换，保持模式切换事件、视口 preview 清理、tree projection 更新和右侧 panel 更新路径一致。
- 未完成业务模式统一使用 `RobotQtViewerRightPanelKind::Status`，避免制造临时业务面板或把未来语义硬塞进 `MainWindow`。
- `DigitalTwin` 新增为 workbench/domain 枚举和描述符，但仅作为 GUI mode 占位；真实连接、遥测、安全策略以后必须落在 Platform/Core service 或 runtime adapter。

## 验证方法

```bat
git -c safe.directory=D:/program/src/RS2026_CodexDev diff --check
cmake --build build --config Release --target RobotQtViewer
```

可选 smoke：

```bat
& .\build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600.sys.json --profile-exit-ms 800
```

## 停止条件

- 需要新增真实业务底层 service 或改变 project schema。
- 需要修改超过 12 个源/配置文件。
- 需要新增第三方依赖。
- `RobotQtViewer` 连续两次构建失败且原因不同。
