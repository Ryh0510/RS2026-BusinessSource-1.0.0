# Robot Run 碰撞结果面板与项目加载性能修改方案

生成时间：2026-07-06

## 1. 背景与目标

用户反馈两个问题：

1. 直接打开程序并加载 `config/projects/420-red4600-tool.sys.json` 时间明显偏长。预期它应接近默认模型加载耗时。当前怀疑慢点来自加载时生成碰撞检测模型或构建碰撞 detector runtime。
2. 原先碰撞检测模式右侧的碰撞结果显示不应继续放在配置模式里，应迁移到 `Robot Run` 模式。`Robot Run` 中继续保留手动关节调节、自动关节运动、设置当前位姿为初始角度，并在下方增加碰撞 detector 选择、启用/停用和结果显示。

本方案只规划后续修改，不直接修改功能代码。

## 2. 当前代码观察

### 2.1 加载性能路径

相关路径：

- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
  - `openProjectPathForProfiling(...)`
  - `reloadViewportProject()`
- `SMRobotApps/RobotViewerCore/RobotViewport.cpp`
  - `RobotViewport::loadProjectDocument(...)`
  - `RobotViewport::initializeSceneWithCurrentContext(...)`
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`
  - `ProjectScene::initialize(...)`
  - `ProjectScene::Impl::buildProjectToolAttachments()`
  - `ProjectScene::Impl::applyProjectCollisionAndView()`
  - `ProjectScene::rebuildCollisionDetectorsFromDocument(...)`
- `SMRobotApps/RobotViewerCore/ProjectRuntimeBuilder.cpp`
  - scene object/point cloud collision mesh conversion 和 FCL BVH 构建 profile
- `SMRobotApps/RobotViewerCore/ProjectCollisionDetectorRuntime.cpp`
  - `ProjectCollisionDetectorBuilder::build(...)`

已有 profile 输出点：

- `RobotViewport loadProjectDocument`
- `Scene OpenGL runtime`
- `Scene build robots`
- `Scene attachment graph`
- `Scene build objects`
- `Scene build point clouds`
- `Scene build tool visuals`
- `Scene collision/view`
- `Scene initialize total`
- 对象级：
  - `AssetManager load`
  - `mesh convert`
  - `FCL BVH build`
  - `collision setup`

本轮源码检查发现，`config/projects/420-red4600-tool.sys.json` 与对比项目的大致差异为：

| 项目 | robots | objects | toolAssets | attachments | detectors | pairGenerators |
|---|---:|---:|---:|---:|---:|---:|
| `420-red4600-tool.sys.json` | 2 | 1 | 1 | 1 | 1 | 6 |
| `420-red4600.sys.json` | 2 | 1 | 0 | 0 | 1 | 1 |
| `420.v3.scene.json` | 1 | 1 | 1 | 1 | 1 | 1 |

`420-red4600-tool.sys.json` 额外包含：

- `data/Spray420/420-tool.STL`，约 8.19 MB。
- 1 个 mounted tool attachment：`420_tool_attachment`。
- detector `default_collision` 启用 `contacts / nearestPoints / distance`，并有 6 个 `pairGenerators`，包含 attachment 与多个 link/robot pair。

初步怀疑优先级：

1. `buildProjectToolAttachments()` 加载 tool STL 并为 attachment 建 visual/collision runtime。
2. attachment 参与 detector pair 时，`simulation_runtime::projectReferencesAttachmentCollision(...)` 可能触发 attachment collision object 构建。
3. `ProjectCollisionDetectorBuilder::build(...)` 把 6 个 `pairGenerators` 展开成 include pairs。
4. 如果加载后立刻启用 collision query，则第一帧 `checkCollision + distance` 会因为 `contacts + nearestPoints + distance` 全开而产生额外耗时。

注意：本轮尝试在当前 Codex GUI 沙箱中运行：

```bat
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600-tool.sys.json --profile-enable-collision --profile-exit-ms 800
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600.sys.json --profile-enable-collision --profile-exit-ms 800
```

两次都快速返回 `exit 1` 且没有 stdout/新日志，可能是当前沙箱 GUI 启动环境问题。因此后续执行阶段需要先保证 profile 命令可在开发机正常输出，或把 profile 写入稳定日志文件。

### 2.2 Robot Run 面板现状

相关文件：

- `SMRobotApps/RobotQtModules/MotionControl/MotionControlWidget.h`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlWidget.cpp`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlModuleController.h`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlModuleController.cpp`

当前 UI：

- 标题：`Joint Control`
- robot label
- `Auto Motion` checkbox
- `Amp` / `Speed` 输入
- 关节滚动列表
- 底部按钮：`Apply as Initial Pose`

当前问题：

- `Apply as Initial Pose` 在关节列表之后，视觉上较低，Robot Run 下方没有空间承载碰撞结果。
- `MotionControlWidget` 目前只负责 motion 控件，没有分区或内嵌 collision result 控件。

### 2.3 碰撞结果面板现状

相关文件：

- `SMRobotApps/RobotQtModules/CollisionRuntimeResults/CollisionRuntimeResultsWidget.*`
- `SMRobotApps/RobotQtModules/CollisionRuntimeResults/CollisionResultsWidget.*`
- `SMRobotApps/RobotQtModules/CollisionRuntimeResults/CollisionResultViewController.*`
- `SMRobotApps/RobotQtModules/Shared/CollisionRuntimeViewModel.h`

当前能力：

- 显示 detector summary。
- 显示 contacts 表：body A、body B、position、normal、depth。
- 显示 nearest points 表：body A、body B、point A、point B、distance。
- 从 `RobotQtViewerViewportServices::collisionRuntimeDetectors()` 读取 runtime detector 信息。

当前绑定：

- `CollisionWorkbenchModuleController` 内部持有 `CollisionResultViewController`。
- 结果面板之前挂在 collision config panel 的 `Results` tab。
- 上一轮已经从 collision config 可见 UI 中隐藏了 `Results`，但对象仍存在，供后续迁移复用。

## 3. 设计原则

### 3.1 加载性能

加载性能问题属于 runtime/render/collision 初始化路径，不应通过 GUI 层隐藏等待或减少刷新假象解决。

应先用 profile 证明慢点来自哪个阶段：

- 文档 IO。
- robot 加载。
- object/attachment STL 加载。
- collision mesh 转换。
- FCL BVH 构建。
- detector pair 展开。
- 首帧 collision query。

只有确认 root cause 后，再决定优化：

- 延迟构建 detector runtime。
- 延迟首帧 distance/nearest query。
- 缓存 attachment collision mesh。
- 避免未启用 Robot Run/Collision Query 时构建不必要 detector。
- 或调整项目配置中 detector 默认启用策略。

### 3.2 Robot Run 中的碰撞结果

Robot Run 是运行态观察/调试位置，适合显示 detector 运行结果，但不应拥有 detector 配置语义。

建议数据流：

```text
Robot Run detector selector / enable toggle
    -> Motion/Run module controller emits collision runtime intent
    -> viewport services setActiveCollisionDetector / setCollisionDetectorEnabled / setCollisionQueriesEnabled
    -> RobotViewport / ProjectScene 执行 query
    -> CollisionResultViewController 从 viewport runtime snapshot 读取结果
    -> CollisionResultsWidget 显示 summary/contact/nearest
```

配置仍归属于 Collision Detector Configuration：

```text
Collision Config mode
    -> 编辑 detector query / pair scope / geometry role
    -> document mutation
    -> rebuild detector runtime
```

Robot Run 只选择使用哪个 detector、是否启用查询、显示当前结果。

## 4. 修改范围

### 4.1 加载性能调查与 profile 稳定化

预期修改文件：

- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `SMRobotApps/RobotViewerCore/RobotViewport.cpp`
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`
- `SMRobotApps/RobotViewerCore/ProjectRuntimeBuilder.cpp`
- `SMRobotApps/RobotViewerCore/ProjectCollisionDetectorRuntime.cpp`

计划：

1. 增加或修复 `--profile-project` 输出落点：
   - stdout 保留；
   - 同时写 `log/profile_project_load_<timestamp>.txt`，避免 GUI 环境无法捕获 stdout。
2. 给 `openProjectPathForProfiling` 增加失败原因输出：
   - Qt 启动成功但 load 失败；
   - project IO 失败；
   - viewport reload 失败；
   - profile 退出码来源。
3. 在 `ProjectScene::rebuildCollisionDetectorsFromDocument(...)` 增加耗时统计：
   - detectors 数量；
   - pairGenerators 总数；
   - includePairs/effectivePairs 数量；
   - elapsedMs。
4. 在 `buildProjectToolAttachments()` 增加和 scene object 一致的 profile：
   - asset path resolve；
   - AssetManager load；
   - render visual build；
   - collision mesh convert；
   - FCL BVH build；
   - collision object register；
   - total。
5. 跑三组对比：

```bat
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420.v3.scene.json --profile-exit-ms 800
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600.sys.json --profile-enable-collision --profile-exit-ms 800
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600-tool.sys.json --profile-enable-collision --profile-exit-ms 800
```

6. 根据 profile 结果分类修复：
   - 如果 `Scene build tool visuals` 慢：优先查 tool STL 加载和 mounted attachment collision build。
   - 如果 `Scene collision/view` 慢：优先查 detector runtime build 和 pair expansion。
   - 如果首帧 query 慢：Robot Run 中应默认选择 detector 但不立即启用 expensive distance/nearest，或按用户 toggle 启用。

### 4.2 Robot Run 面板压缩

预期修改文件：

- `SMRobotApps/RobotQtModules/MotionControl/MotionControlWidget.h`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlWidget.cpp`

计划：

1. 把 `Apply as Initial Pose` 从关节滚动列表下方上移到自动运动参数区。
2. 建议布局：

```text
Joint Control
Robot: <id>
[ ] Auto Motion
Amp ...      Speed ...
[Apply as Initial Pose]
------------------------------------------------
<joint rows scroll area>
------------------------------------------------
Collision Detector
Detector: [combo................] [Enabled]
<result summary>
<contacts table>
<nearest table>
```

3. 控件压缩策略：
   - `Auto Motion`、`Amp`、`Speed`、`Apply as Initial Pose` 形成 compact top block。
   - 关节列表保留滚动区域，但最大高度可限制，给下方 collision result 留空间。
   - 使用 `QFrame::HLine` 或已有主题边框做视觉分隔，不做嵌套 card。

### 4.3 Robot Run 增加 detector 选择与启用

建议新增一个小控件，避免 `MotionControlWidget` 承担 collision 选择细节：

- 新文件：
  - `SMRobotApps/RobotQtModules/MotionControl/RunCollisionMonitorWidget.h`
  - `SMRobotApps/RobotQtModules/MotionControl/RunCollisionMonitorWidget.cpp`

职责：

- 显示 detector 下拉框。
- 显示启用/停用 checkbox 或 toggle。
- 内嵌 `CollisionResultsWidget`。
- 发出用户意图：
  - `detectorSelectionChanged(detectorId)`
  - `detectorEnabledChanged(detectorId, enabled)`

不负责：

- 修改 project detector 配置；
- 生成 detector；
- 修改 pair scope；
- 修改 collision geometry；
- 直接访问 `ProjectScene`。

### 4.4 Robot Run controller 接入 runtime detector

预期修改文件：

- `SMRobotApps/RobotQtModules/MotionControl/MotionControlModuleController.h`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlModuleController.cpp`
- `SMRobotApps/RobotQtModules/MotionControl/CMakeLists.txt`
- 可能新增共享 view model：
  - `SMRobotApps/RobotQtModules/Shared/CollisionRuntimeDetectorSelectorViewModel.h`

计划：

1. `MotionControlModuleController` 接收 document/runtime event 后刷新 detector 下拉：
   - 从 `RobotQtViewerViewportServices::collisionRuntimeDetectors()` 读取 detector id/name/enabled/active。
   - 当前 active detector 优先来自 `InspectorContext::activeCollisionDetectorId()`，不存在则选第一个 detector。
2. 用户切换下拉：
   - 调用 `viewportServices->setActiveCollisionDetector(detectorId)`。
   - 更新 app inspector context：需要通过新的 callback/service 注入，而不是让 Motion 模块直接知道 `RobotQtViewerAppController`。
3. 用户启用/停用：
   - 调用 `viewportServices->setCollisionDetectorEnabled(detectorId, enabled)`。
   - 当任一 detector 启用时，确保 `viewportServices` 或 `RobotViewport` 的 collision query 总开关打开。
   - 当用户显式全部停用时，只显示动画，不更新 collision result。
4. `RobotStateUpdated` 时按节流刷新结果：
   - 每 6 帧或 10 Hz 刷新一次 `CollisionResultsWidget`。
   - 避免每帧重建表格导致 Robot Run 卡顿。
5. 重用 `CollisionResultViewController` 或抽出更通用的 `CollisionRuntimeResultController`：
   - 输入：detector id、viewport service、collision pair marker。
   - 输出：`CollisionResultsViewModel` 给 widget。

### 4.5 MainWindow 接线

预期修改文件：

- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `SMRobotApps/RobotQtViewer/MainWindow.h`
- `SMRobotApps/RobotQtViewer/RobotQtViewerViewportServicesAdapter.*`
- 如需 app context callback，可新增/扩展 Motion app services adapter。

计划：

1. 创建 `RunCollisionMonitorWidget` 作为 `MotionControlWidget` 的子控件，或由 `MotionControlWidget` 提供 slot 暴露结果区域。
2. 将现有 `CollisionResultsWidget` 从 Collision Config 私有拥有迁移到 Robot Run：
   - Collision Config 继续保留 detector 配置；
   - Robot Run 创建自己的 result widget/controller；
   - 不共享同一个 QWidget 实例，避免 Qt parent 和生命周期冲突。
3. 在 `MainWindow::refreshCollisionDetectorDetails()` 中区分：
   - Collision Config 不再刷新可见 result；
   - Robot Run 刷新 run collision monitor。
4. 在项目加载或 detector 配置变化后：
   - Robot Run detector 下拉重建；
   - 如果 active detector 被删除，回退到第一个 detector 或空状态。

## 5. 加载性能可选修复策略

最终修复必须由 profile 结果决定。可选策略如下。

### 策略 A：加载时不立即执行 expensive collision query

适用条件：

- profile 显示加载本身不慢，但 `--profile-enable-collision` 或进入 Robot Run 后首帧 query 慢。

做法：

- 加载项目只构建 detector runtime，不执行 contacts/distance。
- Robot Run 中用户勾选 `Enabled` 后再打开 query。
- distance/nearest 按已有节流策略运行。

风险：

- 需要清楚区分 detector enabled 和 global collision queries enabled。

### 策略 B：延迟构建 detector runtime

适用条件：

- `Scene collision/view` 或 `ProjectCollisionDetectorBuilder::build` 明显慢。

做法：

- 加载后只读取 detector 文档描述；
- 第一次进入 Robot Run 或第一次启用 detector 时构建 runtime；
- detector 配置模式编辑后标记 runtime dirty，不立即全量重建。

风险：

- 需要状态机表达 runtime dirty；
- 需要保证 viewport overlay/query 在 runtime 未构建时显示清晰空状态。

### 策略 C：缓存 mounted attachment collision mesh/FCL BVH

适用条件：

- `buildProjectToolAttachments` 中 `AssetManager load`、`mesh convert` 或 `FCL BVH build` 慢。

做法：

- 对 attachment asset path + visualScale + collisionScale 建 cache key；
- 复用 scene object 的 `CachedSceneObjectCollision` 思路；
- 同一 STL 多次加载或 reload 时复用 collision geometry。

风险：

- cache invalidation 必须包含 asset path、scale、override 和 replaceOriginal 策略。

### 策略 D：项目配置层优化

适用条件：

- tool STL 高面数且 exact collision 不必要。

做法：

- 将 detector geometry role 默认改为 `PlanningProxy` / `Simplified` / `SphereCover`；
- 或在项目文件中为 tool attachment 提供简化 collision override。

风险：

- 这是项目语义变更，不应由 GUI 隐式完成；
- 需要用户明确接受精度与性能权衡。

## 6. 验证计划

### 6.1 构建验证

```bat
git -c safe.directory=D:/program/src/RS2026_CodexDev diff --check
cmake --build build --config Release --target RobotQtModulesMotionControl RobotQtModulesCollisionRuntimeResults RobotQtViewer
```

### 6.2 Profile 验证

目标：确认 `420-red4600-tool.sys.json` 的加载耗时接近对比项目，或至少定位剩余慢点。

```bat
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420.v3.scene.json --profile-exit-ms 800
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600.sys.json --profile-enable-collision --profile-exit-ms 800
build\Release\bin\RobotQtViewerrx64.exe --profile-project config\projects\420-red4600-tool.sys.json --profile-enable-collision --profile-exit-ms 800
```

记录：

- total load ms；
- scene build robots ms；
- scene build objects ms；
- scene build tool visuals ms；
- scene collision/view ms；
- detector runtime build ms；
- first collision query ms；
- FCL BVH build ms。

### 6.3 Robot Run UAT

1. 打开 `config/projects/420-red4600-tool.sys.json`。
2. 进入 `Robot Run` 模式。
3. 确认上方 motion 控件紧凑：
   - Auto Motion 可用；
   - Amp/Speed 可用；
   - Apply as Initial Pose 在上方区域；
   - 关节手动调节仍正常。
4. 在下方 detector 区：
   - 下拉框列出 project detector；
   - 选择 detector 后 active detector 切换；
   - 勾选 Enabled 后开始显示碰撞结果；
   - 取消 Enabled 后只显示动画，不再更新碰撞结果。
5. 在配置为 contacts/normals/nearest/distance 的 detector 下：
   - 接触时 summary 显示碰撞；
   - contact table 有 body/position/normal/depth；
   - 非接触时 nearest table 显示两点和 distance；
   - viewport 高亮、接触点、法线、最近点连线与 detector visualization 设置一致。
6. 回到 Collision Config 模式：
   - 不显示运行结果页；
   - 仍可配置 detector query、pair scope、geometry。

## 7. 非目标

- 不在本阶段重做 detector 配置 UI。
- 不修改 project JSON schema，除非 profile 证明必须新增明确的 runtime policy 字段。
- 不改变 Collision core 的 contacts/distance API。
- 不引入新第三方依赖。
- 不把 Robot Run 变成 detector 编辑器；Robot Run 只选择、启用和显示 detector runtime 结果。

## 8. 风险与停止条件

风险：

- 如果 MotionControlWidget 直接持有 detector 语义，后续会导致 Robot Run 和 Collision Config 行为分裂；应使用 controller/service 层处理 runtime intent。
- 如果加载慢来自 asset/FCL 构建，单纯迁移 UI 无法解决性能。
- 如果 `enabled` 同时表示 project detector 默认启用和 Robot Run 当前启用状态，需要拆清楚持久配置与运行态开关。

停止条件：

- 需要修改超过 12 个源/配置文件。
- 需要修改 project schema。
- 需要重命名 public API 或移动模块。
- profile 结果无法采集，且无法确认慢点。
- 两次构建因不同原因失败。

## 9. 建议执行顺序

1. 先做 profile 稳定化和性能定位，不改行为。
2. 根据 profile 做最小性能修复。
3. 抽出/复用 collision runtime result controller。
4. 重排 Robot Run motion UI。
5. 在 Robot Run 下接 detector selector、enabled toggle 和 result widget。
6. 移除 Collision Config 中剩余 result 私有持有关系，或保留为不可见兼容对象直到 Robot Run 迁移完成后再清理。
7. 构建、profile、UAT。
