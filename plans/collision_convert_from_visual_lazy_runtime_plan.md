# Convert from Visual 懒转换运行语义修订方案

## 1. 结论

当前实现还没有完成用户期望的运行语义。

已经完成的是 UI 层第一阶段：

- `Collision Model Configuration` 面板可以显示 `Convert from Visual` 候选。
- 用户选择该候选时，视口先高亮对应 link 的视觉模型。
- 点击 `Set Current` 后，项目文档可以保存一个当前 `activeModelId`。

尚未完成的是底层运行语义：

- robot run、path planning、collision detector runtime 等真正构建碰撞对象的路径，目前还没有统一识别 `Convert from Visual` 选择。
- 当前 UI 补出的 `visual|robot|link` 候选只是界面层临时 id，不应长期作为底层稳定协议。
- `ProjectCollisionRuntime::buildRobotCollisionModel()` 仍主要读取 `robotModel.links[link].collisions`，没有按 `robotLinkModelSelections` 决定是否从 visual geometry 懒转换。

用户提出的期望是合理的：如果某个 link 被实际纳入碰撞检测、且当前选择是 `Convert from Visual`，则运行时先检查该 link 的 visual-based collision 是否已经存在；不存在时再在该阶段从视觉模型生成碰撞模型，并缓存复用。

## 2. 目标

建立 `Convert from Visual` 的正式运行语义：

1. `Convert from Visual` 是项目文档中的一种稳定 collision model selection，不只是 UI preview。
2. 该选择不要求在配置面板打开时立即生成复杂 mesh collision。
3. 当 robot run、path planning、trajectory planning、digital environment、collision detector runtime 等模块实际引用该 link 时，才懒加载/懒转换视觉模型。
4. 转换结果应进入对应运行时 collision model / collision object，并参与真实碰撞检测。
5. 原始 URDF / Simscape collision 模型仍必须保留，不因选择 visual fallback 而删除。
6. project-defined 简化模型仍可作为另一种 variant 被选择；选择它时不走 visual mesh 转换。

## 3. 非目标

本轮方案不实现以下能力：

- 不把 visual fallback 永久写回 URDF / Simscape 原文件。
- 不把 visual fallback 自动保存成 project-defined box/sphere/convex override。
- 不实现 convex decomposition、sphere cover、mesh simplification 等新生成算法。
- 不改变 detector 配置面板的结构。
- 不恢复 `Models / Detectors / Results` 三段式右侧导航。

## 4. 当前数据流

当前第一阶段 UI 数据流：

```text
左侧树 link 右键 Configure Collision Model
    -> CollisionLinkModelsController 构建 view model
    -> 如果 runtime summary 没有真实 visual/original variant，则 UI 补一个 Convert from Visual 候选
    -> 用户点击 Set Current
    -> ProjectDocumentService::setActiveRobotLinkCollisionModel(robotId, linkName, activeModelId)
    -> collision.robotLinkModelSelections 保存 activeModelId
```

当前运行时碰撞构建主要数据流：

```text
ProjectCollisionRuntime::build()
    -> ProjectCollisionDetectorBuilder::collectBuildPlan(document)
    -> 对被 detector 引用的 robot 调用 buildRobotCollisionModel(robot.model, includedLinks)
    -> buildRobotCollisionModel 读取 robotModel.links[link].collisions
    -> 生成 RobotCollisionModel / RobotCollisionInstance
```

缺口：运行时构建没有读取 `collision.robotLinkModelSelections`，也没有识别 visual selection。

## 5. 目标数据流

目标运行时数据流：

```text
Project document 中每个 link 可保存 active collision model selection
    -> Runtime build plan 判断哪些 robot/link 被 detector 或上层任务引用
    -> RobotCollisionModelResolver 按 link 解析当前有效 collision source
        -> Defined in Project：使用项目/system.json/sidecar 中定义的简化 override
        -> Imported Collision：使用 URDF / Simscape 自带 collision
        -> Convert from Visual：需要时从 visual geometry 懒转换 triangle mesh collision
    -> RobotCollisionModel / CollisionScene 使用解析后的 collision objects
```

核心原则：

- UI 只发出“选择了哪个 variant”的意图。
- Project document 保存稳定选择。
- Runtime / Platform 层负责把选择解析成真实碰撞对象。
- 转换发生在运行时真正需要该 link 时，而不是配置面板打开时。

## 6. 稳定选择协议

建议不要继续把 `visual|robot|link` 作为长期底层协议。该字符串是 UI 阶段为了显示候选临时生成的 id。

建议新增一个集中 helper，定义 link-scoped 的稳定 selection id：

```text
convertFromVisual
```

由于 `RobotLinkCollisionModelSelectionDesc` 已经包含 `robotId` 和 `linkName`，`activeModelId` 不需要再编码 robot/link。

兼容策略：

- 新写入使用 `convertFromVisual`。
- 读取时临时兼容旧的 `visual|<robot>|<link>`，把它解释为 `convertFromVisual`。
- 后续保存项目时可自然写回新 id。

建议位置：

- `SimulationProject` 中增加轻量 helper，例如 `RobotLinkCollisionModelSelectionIds` 或同等函数。
- Qt UI、ProjectDocumentService、Runtime resolver 都使用同一个 helper，避免散落字符串判断。

## 7. 运行时解析规则

每个 link 的有效 collision source 按以下优先级解析：

1. 如果存在 link-level `activeModelId`：
   - `convertFromVisual`：使用该 link 的 visual geometry 懒转换 collision mesh。
   - project-defined model id：使用对应 override element / variant。
   - imported/original model id：使用导入模型自带 collision。
2. 如果不存在 link-level selection：
   - 保持当前默认策略。
   - 若 robot override `replaceOriginalCollisions=true`，继续优先使用 project-defined override。
   - 否则使用导入 collision。
   - 如果导入 collision 不存在，且系统默认允许 visual fallback，则从 visual geometry 懒转换。

注意：用户明确选择 `convertFromVisual` 时，即使 project 中有简化 box override，也应以 visual selection 为准；否则用户无法在两个 variant 之间真正切换。

## 8. 懒转换与缓存

`Convert from Visual` 不应在配置面板中构建复杂碰撞模型。

建议在 Runtime / Platform 层建立缓存：

```text
cache key:
    robot source path / robot document id
    link name
    visual geometry identity
    visual mesh path
    scale
    local transform
```

触发条件：

- detector build plan 或上层 runtime 任务引用了该 robot/link。
- link 当前 selection 是 `convertFromVisual`。
- 当前 runtime cache 中没有对应 visual-based collision geometry。

缓存结果：

- `CollisionGeometryPtr`
- mesh bounds / vertex count / index count
- shape metadata，role 建议仍为 `Exact`
- source metadata 标记为 `ConvertFromVisual`

## 9. 预计修改文件

需要先进一步确认 RobotModel 中 visual geometry 的实际数据结构，然后再定最终文件列表。预计涉及：

- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
  - 可能不改 schema，只保留现有 `activeModelId`。
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`
  - 写入 `convertFromVisual` helper。
  - 读取时兼容 `visual|robot|link`。
- `SMRobotPlatform/SimulationRuntime/src/ProjectCollisionRuntime.cpp`
  - `buildRobotCollisionModel()` 接入 document selection 和 visual fallback resolver。
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`
  - 视口 collision debug draw / lazy build 路径需要与 runtime resolver 保持一致。
- `SMRobotApps/RobotViewerCore/ProjectRuntimeBuilder.cpp`
  - 如果 Qt viewer 的 lazy collision build 仍有独立路径，需要迁移到同一解析逻辑或调用共享 helper。
- `SMRobotApps/RobotQtModules/CollisionLinkModelSetup/CollisionLinkModelsController.cpp`
  - UI 候选 id 改为稳定 `convertFromVisual`。
- `SMRobotApps/RobotQtModules/CollisionLinkModelSetup/CollisionRequestWorkbenchController.cpp`
  - preview 仍可高亮 visual link，但 Set Current 写入稳定 id。
- 回归测试：
  - 优先扩展 `SMRobotApps/RobotViewerCore/regression/CollisionModelWorkflowSmokeTest/main.cpp`。
  - 如 SimulationRuntime 已有适合测试入口，则增加 headless runtime 测试。

## 10. 验证方案

最小回归：

1. 构造或复用一个 robot link：有 visual mesh、无 imported collision。
2. 设置 `robotLinkModelSelections`：该 link 的 `activeModelId=convertFromVisual`。
3. 构造 detector，使该 link 被纳入检测范围。
4. 构建 runtime collision。
5. 断言该 link 生成了 collision object，且 metadata/source 表明来自 visual fallback。
6. 构造另一个 detector，不引用该 link。
7. 断言该 link 不触发 visual mesh conversion，证明懒构建成立。
8. 对 project-defined override 再测一条：同一 link 选择 project-defined model 时，不走 visual conversion。

命令建议：

```bat
cmake --build build --config Release --target RobotQtViewer
cmake --build build --config Debug --target RobotViewerCore-CollisionModelWorkflowSmokeTest
ctest --test-dir build -C Debug -R CollisionModelWorkflowSmokeTest --output-on-failure
```

实际命令以当前 CMake target 名称为准。

## 11. 风险与停顿条件

需要停下来讨论的情况：

- RobotModel 当前没有足够的 visual geometry 信息可供 runtime headless 转换。
- visual mesh 只存在于 RenderCore / Qt viewer 路径，导致 Runtime 层拿不到数据。
- 为了支持 visual fallback 必须新增跨模块依赖或引入新第三方库。
- 需要修改 project schema，而不是仅使用现有 `activeModelId`。
- ProjectScene 和 ProjectCollisionRuntime 存在两套不可合并的 collision build 路径，改动超过 12 个源文件。
- 两次构建失败且失败原因不同。

## 12. 最小正确设计

最小正确设计不是在 Qt 面板里生成 collision mesh，也不是在按钮回调里直接塞一个临时 collision object。

正确归属应是：

```text
ProjectDocument
    保存用户选择

SimulationProject / helper
    定义稳定 selection id 与兼容解析

SimulationRuntime / RobotViewerCore runtime bridge
    在实际需要碰撞对象时解析 selection，并懒构建 visual collision

Qt Widget / Controller
    只展示候选、预览、高亮，并发送 Set Current 意图
```

这样 robot run、path planning、trajectory planning、realtime simulation 和后续非 Qt 调用路径才能使用同一套 collision model selection 语义。
