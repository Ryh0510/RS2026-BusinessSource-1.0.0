# Workbench GUI 与领域功能包分层架构

## 1. 文档目的

本文把当前机器人仿真平台下一阶段的结构目标固化为长期架构准则：

- `SimWorkbench` 下的各个 Workbench 是 Qt GUI 工作模式包。
- 与某个工作模式专属的 Widget、Dialog、Qt Controller、ViewModel 和界面适配器应归入对应 Workbench 包。
- 轨迹规划、喷涂、涂层分析、碰撞、机器人运行等真实业务能力和算法不得由 Qt Workbench 拥有。
- 真实业务能力应由可独立构建、安装和预编译消费的非 Qt Package 提供。
- `RobotQtViewer` 只作为应用壳层和组合根，不应继续吸收具体业务模式实现。

本文是架构约束，不直接执行源码迁移。具体实施前应根据当前源码重新制定计划，并登记到 `plans/README.md`。

## 2. 对需求的准确理解

用户期望的最终关系不是“把所有代码都搬进 Workbench”，而是两层独立模块协作：

```text
Qt Workbench Package
    负责界面、对话框、交互状态、模式组合和用户意图
                |
                v
Non-Qt Domain Package
    负责数据契约、业务规则、算法、服务和可测试结果
```

因此，一个 Workbench 可以包含完整的 Qt 交互实现，但不能把算法和真实项目语义私有化。即使没有 `RobotQtViewer`，同一领域 Package 仍应能被 SDK、命令行工具、测试、`RobotGlfwViewer` 或第三方程序调用。

## 3. 当前源码事实

### 3.1 已完成的部分

- `ProjectAssembly`、`CollisionConfig`、`RobotRun` 的真实 Qt 组件已经迁入 `SimWorkbench`。
- `SceneExplorer` 和 `ToolSetup` 由 `SMRobotWorkbenchProjectAssembly` 拥有。
- collision 配置、collision link model 和 collision 结果 UI 由 `SMRobotWorkbenchCollisionConfig` 拥有。
- `MotionControl` 由 `SMRobotWorkbenchRobotRun` 拥有。
- `RobotQtViewer` 已通过 Workbench 聚合 target 消费上述组件。

### 3.2 尚未收敛的部分

- `SMRobotApps/RobotQtModules/Dialogs` 当前是空目录，并没有尚待搬迁的一组 Dialog 源码。
- 当前可识别的独立业务 `QDialog` 类是 `CollisionDetectorQueryDialog`，它已经位于 `SMRobotWorkbenchCollisionConfig`。
- `MainWindow.cpp` 仍直接发起多种 `QFileDialog`、`QInputDialog` 和 `QMessageBox`。其中既有项目级 open/save，也有 Project Assembly、Collision Config 和 viewport export 等模式相关交互。
- `RobotQtModules/CoreWidgets` 同时包含真正通用的 `RobotQtWidgetUtils` 和明显偏 Project Assembly 的 `ToolAssetEditorWidget`、`ToolTransformEditorWidget`。
- `RobotQtModules/Shared` 同时包含 Workbench 基础设施、app controller、project session workflow、viewport service，以及多组 collision facade/controller/view-model，公共包边界过宽。
- `MotionPlanningWorkbench` 当前仍是 interface shell，只依赖 `WorkbenchCommon` 和 `RobotTrajectoryCore`，尚未拥有真实 GUI 子组件。
- 当前已有非 Qt `RobotTrajectoryCore`，但它只提供轨迹基础数据能力，不能自动等同于完整的 Motion Planning 算法包。
- `SMRobotSpray` 已经证明“非 Qt 领域 Package 独立存在”这一结构可行，其下已有喷涂核心、路径规划、厚度预测和轨迹优化组件。

## 4. 目标分层

### 4.1 应用壳层

`SMRobotApps/RobotQtViewer` 负责：

- `main()`、`MainWindow`、窗口布局、菜单、全局 toolbar 和状态栏。
- Workbench package 的发现、装配、启用和生命周期管理。
- 项目级 open/save/close 等全应用交互入口。
- 将 app-level service adapter 注入 Workbench。
- Qt 主题、语言和全局窗口设置。

它不负责：

- 轨迹规划算法、喷涂算法、碰撞规则或运行控制规则。
- 某个 Workbench 专属 Dialog、Panel 或 ViewModel 的长期实现。
- 通过 `MainWindow` 分支直接实现具体业务命令。

### 4.2 Workbench GUI 包

每个 `SMRobotWorkbench<Mode>` 包负责一个 Qt 工作模式。建议内部按功能组织，而不是建立一个全包共享的巨大 `Dialogs` 目录：

```text
SimWorkbench/
    SMRobotWorkbenchMotionPlanning/
        MotionPlanningWorkbench/
            Widgets/
            Dialogs/
            Controllers/
            ViewModels/
            Adapters/
            Contributions/
```

实际目录可以根据组件规模合并；分类表达所有权，不要求立即建立所有空目录。

Workbench 可以拥有：

- Qt Widget、Dialog、Dock、Panel 和 delegate。
- Qt ViewModel、selection projection 和临时编辑草稿。
- ModuleController、WorkbenchController 和 mode lifecycle。
- 用户输入校验与显示格式转换。
- 将 Qt 信号转换为非 Qt command/request 的 adapter。
- action、menu、panel、tree projection 和 viewport overlay contribution。

Workbench 的 Qt GUI 组件不应成为以下内容的唯一权威所有者：

- 规划器、优化器、碰撞求解器或厚度预测算法。
- Workbench 可以拥有基于 Qt 的表单草稿和 ViewModel，但领域服务的正式 request/result 应具有不依赖 Qt 的表达。
- Workbench 可以发起并展示项目修改，也可以拥有 GUI 会话状态；但领域项目 schema、统一保存加载规则、真实 runtime 状态和稳定实体身份，应由 project/session/runtime 或领域 Package 维护。
- Widget 不应绕过 Workbench Controller 或正式的 project/runtime command API 直接修改底层状态；Workbench Controller 可以通过这些公开接口完成项目和运行时修改。

### 4.3 非 Qt 领域 Package

领域 Package 应位于 `SimWorkbench` 之外，与 `SMRobotCore`、`SMRobotPlatform`、`SMRobotSpray` 同级或按现有包结构放置。它负责：

- 使用标准 C++ 类型表达 request、constraint、result、diagnostic 和 stable id。
- 领域算法、策略接口和算法实现。
- 可取消任务、进度、错误和确定性结果契约。
- 对 Core/Platform 服务的组合，但不依赖 Qt、Widget 或 `RobotQtViewer`。
- 可独立安装的 headers、libraries 和 CMake targets。
- headless 单元测试、算法测试和 package consumer 示例。

### 4.4 Platform bridge 和可视化服务

算法结果通常不能直接变成 Qt Widget，也不应直接持有 OpenGL 对象。推荐路径是：

```text
Domain Result
    -> Project/Runtime result entity
    -> Platform visualization bridge
    -> scene/path/heatmap/section/color-bar representation
    -> Workbench overlay contribution
    -> RobotQtViewer viewport
```

模型显示、路径显示、厚度图、颜色条和切面等能力，应由稳定的 Platform/Viewer service 暴露；Workbench 只提交显示意图和结果引用。

## 5. Dialog 与 Qt 实现的归属规则

判断一个 Dialog 放在哪里，应看它服务的用户任务，而不是只看类名。

| Dialog 或调用类型 | 所有者 | 说明 |
|---|---|---|
| Collision detector query | `SMRobotWorkbenchCollisionConfig` | 只服务 collision 配置模式 |
| Tool、attachment、mount、object 编辑 | `SMRobotWorkbenchProjectAssembly` | 属于装配工作流 |
| Motion planning request、constraint、planner 参数 | `SMRobotWorkbenchMotionPlanning` | Qt 表单只构造非 Qt request |
| Spray process 参数与路径预览 | `SMRobotWorkbenchSprayProcess` | 算法仍由 `SMRobotSpray` 提供 |
| Thickness result filter、legend 设置 | `SMRobotWorkbenchPaintingAnalysis` | 厚度结果事实不由 Dialog 持有 |
| Project open/save/close confirmation | `RobotQtViewer` app shell | 影响整个应用会话，不属于单一 mode |
| 通用视觉控件或无业务语义的 Qt helper | `SMRobotWorkbenchCommon` | 必须证明被多个 Workbench 稳定复用 |
| `QFileDialog`、`QMessageBox` 等 Qt 标准对话框 | 由调用场景决定 | Qt 自身没有待迁移源码，应迁移业务调用入口 |

业务 Dialog 中未提交的文本和临时选项可以属于 Widget；Apply 后的事实必须进入领域 request、project/session 或 runtime。

## 6. `RobotQtModules` 的目标收敛

### 6.1 `Dialogs`

当前目录为空。后续应删除空目录，不再恢复“所有业务 Dialog 集中存放”的结构。新的业务 Dialog 直接进入所属 Workbench。

### 6.2 `CoreWidgets`

- `RobotQtWidgetUtils`：保留在真正的 Workbench common Qt utility 组件。
- `ToolAssetEditorWidget`：迁入 `SMRobotWorkbenchProjectAssembly`。
- `ToolTransformEditorWidget`：当前主要服务 Project Assembly，优先一并迁入；未来若 Collision、Planning 等模式形成稳定复用，再抽成窄接口的 common transform editor。

### 6.3 `Shared`

应拆成三类：

- 真正公共的 Qt Workbench 基础设施：event hub、document-view registry、Workbench registry、基础 selection/session view state。
- 业务专属 Qt facade/controller/view-model：移动到对应 Workbench。
- app shell 编排和 project open/save workflow：回到 `RobotQtViewer` 或独立 app-level Qt 组件。

当前 collision 相关 shared 类使用 `QString`、`QVector` 等 Qt 类型，短期可以先迁入 Collision Workbench；长期再把其中真实 collision/project 规则下沉到非 Qt Core/Platform service，Qt facade 只做类型和交互适配。

### 6.4 `Status`

全局状态栏和系统状态保留在 app shell。若未来出现 RobotRun 专属运行监控 Panel，应由 `SMRobotWorkbenchRobotRun` 提供，不与全局 status 混合。

## 7. Motion Planning 的建议落地结构

### 7.1 非 Qt Package

建议新增独立包：

```text
SMRobotMotionPlanning/
    MotionPlanningCore/
    MotionPlanningAlgorithms/      # 有真实算法实现时再建立
    MotionPlanningRuntime/         # 有异步任务/执行会话时再建立
```

第一阶段最小组件 `MotionPlanningCore` 应包含：

- `MotionPlanningRequest`
- `MotionPlanningConstraint`
- `MotionPlanningResult`
- `MotionPlanningDiagnostic`
- `IMotionPlanner` 或等价稳定服务接口
- 与 `RobotTrajectoryCore` 的明确输入输出转换

算法实现可以依赖 `Kinematics`、`Collision`、`RobotTrajectoryCore` 和必要的 project/runtime snapshot，但公开接口不应暴露 Qt 类型。

不要为了目录完整提前创建没有实现的空组件。只有出现真实异步任务、算法插件或多规划器选择需求时，再增加 `MotionPlanningRuntime` 或 algorithm registry。

### 7.2 Qt Workbench Package

建议把现有 shell 最终统一为：

```text
SMRobotWorkbenchMotionPlanning
```

它负责：

- 选择 robot、start、goal、path point 和约束。
- 编辑规划参数并构造 `MotionPlanningRequest`。
- 调用非 Qt planning service。
- 显示 progress、diagnostic 和 result summary。
- 通过 Platform visualization service 显示 path、pose frame、collision marker 和轨迹 preview。
- 将用户确认的规划结果写入 project/session 中的 trajectory entity。

它不直接实现 forward/inverse kinematics、collision checking、sampling、optimization 或 trajectory generation。

### 7.3 与 RobotRun 的关系

`MotionPlanningWorkbench` 负责“生成和编辑可执行轨迹”，`RobotRunWorkbench` 负责“执行、暂停、停止、步进和监控轨迹”。

推荐数据流：

```text
MotionPlanningWorkbench
    -> SMRobotMotionPlanning request/service
    -> planning result / RobotTrajectoryCore trajectory
    -> ProjectSession stores trajectory entity
    -> document event
    -> RobotRunWorkbench selects trajectory
    -> SimulationRuntime executes trajectory
```

`RobotRunWorkbench` 不依赖 Motion Planning 的 Widget 或 Dialog；两者通过非 Qt trajectory/result 契约和 project/runtime entity 协作。

## 8. 依赖方向

允许的依赖方向：

```text
RobotQtViewer
    -> SMRobotWorkbench*
        -> SMRobotWorkbenchCommon
        -> SMRobotMotionPlanning / SMRobotSpray / other domain packages
            -> SMRobotCore
            -> SMRobotPlatform service interfaces as required
```

禁止的方向：

```text
SMRobotCore          -X-> Qt / Workbench
SMRobotMotionPlanning -X-> Qt / Workbench
SMRobotSpray         -X-> Qt / Workbench
SMRobotPlatform core -X-> RobotQtViewer
Workbench A          -X-> Workbench B widget implementation
```

如果两个 Workbench 需要同一结果，应共享非 Qt result contract 或独立的窄 display component，不复制 Widget 业务逻辑。

## 9. 安装与 SDK 契约

- 每个 Workbench package 独立 install，并导出 Qt GUI components。
- 每个领域 package 独立 install，并导出非 Qt components。
- Workbench 的 public headers 不应把 `MainWindow`、私有 Widget 或具体 planner implementation 暴露给 consumer。
- 领域 Package consumer 应能在不查找 Qt 的情况下 configure/build/run。
- `UsingPrebuilt_*` 应允许“领域包 prebuilt + Workbench 源码”和“领域包源码 + Workbench prebuilt”等受支持组合。
- 第三方扩展者应通过 contribution/service API 扩展 Viewer，不修改 `MainWindow`。

## 10. 完成判据

满足以下条件时，本轮架构目标才算真正落地：

- `RobotQtModules/Dialogs` 不再作为业务 Dialog 集散地。
- 所有模式专属 Widget/Dialog/Qt Controller 都能在所属 Workbench package 中找到。
- `RobotQtModules/Shared` 不再包含 collision、tool、planning 等业务专属实现。
- Motion Planning 算法可在无 Qt 的 consumer/test 中运行。
- `MotionPlanningWorkbench` 只通过非 Qt service/request/result 调用规划能力。
- RobotRun 与 Motion Planning 不通过 Widget 互相依赖。
- 路径、厚度图、颜色条和切面可以通过稳定的 Platform visualization service 注入 Viewer。
- 源码、install、using-prebuilt 和外部 consumer 验证矩阵通过。

## 11. 设计阶段非目标（历史）

- 本轮不移动任何源码，不删除目录，不修改 CMake。
- 本轮不决定具体规划算法或引入新的第三方规划库。
- 本轮不把现有所有 Qt controller 一次性改写为非 Qt service。
- 本轮不把 `SMRobotCore`、`SMRobotPlatform` 或 `SMRobotSpray` 做无关重构。

以上内容描述的是本文最初形成设计契约时的边界。实际落地状态和剩余项以当前源码、`docs/agent_project_snapshot.md` 及 `docs/agent_change_audit.md` 为准。

## 12. 已确认的实现约束

- Workbench 可拥有 Qt 表单草稿和 ViewModel；正式领域 request/result 必须具有不依赖 Qt 的表达。
- Workbench 可发起并展示项目修改，也可拥有 GUI 会话状态；项目 schema、统一保存加载、真实 runtime 状态和稳定实体身份由 project/session/runtime 或领域 Package 维护。
- Widget 不得绕过 Workbench Controller 或正式 command API 修改底层状态；Controller 通过公开 project/runtime API 完成修改。
- installed package 在外部 consumer 中应主动解析依赖；在同一 superproject 的混合构建中，如果依赖包明确从源码构建，则应等待源码 target，不提前导入同名 prebuilt target。
