# CollisionConfigMode 配置逻辑收口计划

## 1. 目标

围绕以下三层重新收口碰撞配置逻辑：

```text
Collision Geometry
    负责“每个实体用什么碰撞形状”

Collision Target
    负责“哪些项目实体可以变成 ObjectID”

Collision Detector
    负责“哪些 target 之间生成 pair 并执行查询”
```

GUI 配置主线统一为：

```text
Targets / Sets -> Detector
```

运行结果显示属于路径规划、轨迹规划、实时仿真、数字环境等运行/分析模块，不进入 `CollisionConfigMode` 右侧配置面板。同时强化懒构建策略：只有被启用 detector 实际引用的实体，才构建 collision model / collision object。

## 2. 非目标

- 不重写 FCL backend 或底层 `CollisionScene` 查询算法。
- 不一次性删除旧 `CollisionQueryDesc::robotObjectPairs`，先标记为兼容/迁移路径。
- 不在 `MainWindow` 中新增碰撞业务逻辑。
- 不把 geometry override 和 detector 配置继续混成一套不可区分的面板逻辑。
- 不引入新第三方依赖。

## 3. 当前问题

当前 `CollisionDetectorDesc` 同时支持：

- `selectionSets + queryPolicy`
- `targets`
- `pairGenerators`

这三种方式都能表达“检测谁和谁”，导致 GUI 和开发心智不收口。

同时，当前 collision runtime build 更偏向先把项目中可用碰撞实体建进 `CollisionScene`，再由 detector 过滤查询范围。后续应改为先分析 detector 需求，再构建最小 collision runtime。

## 4. 拟采用的不变量

### 不变量 1：GUI 主路径使用 Sets

普通用户配置碰撞检测时，主要看到：

```text
Collision Sets
    Set A
    Set B

Collision Detector
    Within Set A
    Between Set A and Set B
    All Enabled
```

`targets` 与 `pairGenerators` 保留为内部/高级表达，不作为主 UI 心智。

### 不变量 2：项目实体引用不保存 ObjectID

project document 中保存稳定实体引用，例如：

- robot id
- link name
- object id
- attachment id
- point cloud id

`ObjectID` 只属于 runtime，由 build plan 生成。

### 不变量 3：Detector 先生成 Build Plan，再构建 runtime collision objects

新流程应是：

```text
enabled CollisionDetectorDesc
    -> CollisionDetectorBuildPlan
    -> required CollisionTargetRef / GeometryRequest
    -> minimal RuntimeCollisionRobot / RuntimeCollisionObject
    -> includePairs / excludePairs
    -> CollisionScene query
```

### 不变量 4：Geometry 与 Detector 分离

`Collision Geometry` 解决“用什么几何”，`Collision Detector` 解决“检测哪些 pair”。GUI 需要分开展示。

## 5. 需要检查的文件

### 底层 Collision

- `SMRobotCore/Collision/include/Collision/CollisionScene.h`
- `SMRobotCore/Collision/include/Collision/CollisionQueryOptions.h`
- `SMRobotCore/Collision/include/Collision/RobotCollisionModel.h`
- `SMRobotCore/Collision/include/Collision/RobotCollisionInstance.h`

### SimulationRuntime

- `SMRobotPlatform/SimulationRuntime/include/SimulationRuntime/ProjectCollisionDetectorRuntime.h`
- `SMRobotPlatform/SimulationRuntime/src/ProjectCollisionDetectorRuntime.cpp`
- `SMRobotPlatform/SimulationRuntime/src/ProjectCollisionRuntime.cpp`

### SimulationProject

- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocumentService.h`
- `SMRobotPlatform/SimulationProject/src/ProjectDocumentService.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectValidation.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectIo.cpp`

### Qt Modules

- `SMRobotApps/RobotQtModules/CollisionDetectorConfig/*`
- `SMRobotApps/RobotQtModules/CollisionLinkModelSetup/*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneTreeIntentController.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerViewModel.*`
- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerViewportServices.h`

## 6. 预计修改文件

第一阶段预计修改：

- `SMRobotPlatform/SimulationRuntime/include/SimulationRuntime/ProjectCollisionDetectorRuntime.h`
- `SMRobotPlatform/SimulationRuntime/src/ProjectCollisionDetectorRuntime.cpp`
- `SMRobotPlatform/SimulationRuntime/src/ProjectCollisionRuntime.cpp`
- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`
- `SMRobotPlatform/SimulationProject/src/ProjectValidation.cpp`
- `SMRobotApps/RobotQtModules/CollisionDetectorConfig/*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneTreeIntentController.*`

如果第一阶段超过 12 个源文件，应停止并拆分为 runtime build plan 与 GUI 收口两个阶段。

## 7. 类和函数设计建议

### 新增或整理 CollisionTargetRef

建议位于 `SimulationRuntime` 或 `SimulationProject` 与 runtime 之间的合适位置。

```text
CollisionTargetRef
    kind: Robot / RobotLink / SceneObject / ObjectGroup / Attachment / PointCloud
    robotId
    linkName
    objectId
    objectGroupId
    attachmentId
    pointCloudId
```

用途：

- 替代 GUI 和 runtime 中分散的 target 解析。
- 从 `CollisionSelectionSetMemberDesc`、`CollisionDetectorTargetDesc`、`CollisionPairGeneratorDesc` 统一转换。

### 新增 CollisionDetectorBuildPlan

建议位于 `SimulationRuntime`。

```text
CollisionDetectorBuildPlan
    requiredTargets
    requiredGeometryRoles
    requiredGeometrySources
    detectorPairPlans
```

用途：

- 先遍历 enabled detectors，收集需要构建的碰撞对象。
- 再由 build plan 驱动 `ProjectCollisionRuntime::build(...)` 创建最小 collision scene。

### 整理 ProjectCollisionDetectorBuilder

当前 `ProjectCollisionDetectorBuilder::build(...)` 直接从 document/runtime objects 构建 detector runtime。

目标改为两步：

```text
collectBuildPlan(document.collision.detectors)
buildRuntimeObjects(simulation, buildPlan)
buildDetectorRuntime(document.collision.detectors, runtimeObjects)
```

### GUI Controller 收口

`CollisionDetectorConfigModuleController` 应面向：

- Collision Sets
- Detectors

`CollisionLinkModelSetupModuleController` 应面向：

- Geometry
- Overrides

二者可以仍由 `CollisionWorkbenchModuleController` 组合，但 UI 文案和 tab 结构应明确分层。

## 8. 当前数据流与目标数据流

### 当前数据流

```text
ProjectSimulationRuntime build
    -> 尽量构建项目中 enabled collision entities
    -> ProjectCollisionRuntime::rebuildDetectors
    -> ProjectCollisionDetectorBuilder::build
    -> detector desc 生成 includePairs
    -> CollisionDetectorRegistry query
```

### 目标数据流

```text
ProjectDocument::collision.detectors
    -> enabled detectors
    -> collect CollisionDetectorBuildPlan
    -> 只构建被引用的 robot link/object/attachment/pointCloud collision objects
    -> 为每个 detector 生成 includePairs
    -> query
    -> results / overlay
```

## 9. 分阶段执行计划

### 阶段 1：文档与命名收口

完成：

- 使用 `docs/architecture/collision_detection_configuration_logic.md` 作为设计依据。
- 在现有文档中引用或同步 `Collision Geometry / Target / Detector` 三层术语。
- 标记 `CollisionQueryDesc::robotObjectPairs` 为 legacy 兼容路径，暂不删除。

验证：

- 文档清晰说明 selection set、targets、pairGenerators 的关系。
- 不修改行为。

### 阶段 2：引入 CollisionTargetRef

完成：

- 添加统一 target ref 类型或 helper。
- 把 `SceneTreeIntentController::collisionSelectionSetMemberFromItem(...)` 的转换逻辑逐步迁移到共享 target resolver。
- 把 runtime 中 `CollisionSelectionSetMemberDesc`、`CollisionDetectorTargetDesc`、`CollisionPairGeneratorDesc` 到 `ObjectID` 的转换集中。

验证：

- selection set member 增删行为不变。
- 构建 `RobotQtModulesSceneExplorer`、`RobotQtModulesCollisionDetectorConfig`、`RobotQtViewer`。
- 现有 selection set regression 通过。

### 阶段 3：Detector Build Plan 懒构建

完成：

- 在 `ProjectCollisionDetectorRuntime` 附近增加 build plan。
- 先从 enabled detectors 收集所需 targets / geometry roles / geometry sources。
- 调整 `ProjectCollisionRuntime::build(...)`，只构建 build plan 引用的 collision objects。
- `AllEnabled` detector 保持全量构建语义。

验证：

- 构建 `SimulationRuntime`、`RobotViewerCore`、`RobotQtViewer`。
- 用已有项目验证未引用 link/object 不会生成 collision runtime object。
- 确认 active detector / all detectors 查询结果不变。

### 阶段 4：GUI 配置主线改为 Sets -> Detector

完成：

- 调整 `CollisionWorkbenchPanel` 的 UI 结构或 tab 文案。
- 把 selection set 作为普通用户入口。
- detector 创建默认优先使用 `WithinSet` / `BetweenSets`。
- pair generator 作为高级/快捷创建，不作为主入口。

验证：

- 手动 UAT：创建 Set A / Set B，创建 BetweenSets detector，运行并查看结果。
- 手动 UAT：从两个 link 快捷创建 LinkLink detector，确认仍可工作。

### 阶段 5：Geometry 独立面板化

完成：

- 把 collision geometry variant、proxy generation、override 保存从 detector 配置心智中分离。
- UI 命名明确为 `Geometry` 或 `Collision Geometry`。
- detector 只引用 geometry role/source，不直接混入 proxy 生成流程。

验证：

- proxy generation / save override / export URDF 行为不变。
- detector 切换 geometry role 后 runtime 正确 rebuild。

## 10. 停止条件

出现以下情况应停止并重新讨论：

- 需要修改超过 12 个源文件才能完成当前阶段。
- 需要改变 project schema 字段语义或删除旧字段。
- 需要重命名公共 API。
- 需要改变模块依赖关系。
- 两次连续构建失败且原因不同。
- 懒构建后无法证明查询结果与旧全量构建一致。
- `AllEnabled` detector 的语义变得不确定。
- GUI 中无法解释 `targets`、`selectionSets`、`pairGenerators` 的兼容关系。

## 11. 验证命令

优先执行：

```bat
git -c safe.directory=D:/program/src/RS2026_CodexDev diff --check
cmake --build build --config Release --target SimulationRuntime
cmake --build build --config Release --target RobotQtModulesCollisionDetectorConfig
cmake --build build --config Release --target RobotQtModulesCollisionLinkModelSetup
cmake --build build --config Release --target RobotQtViewer
```

如涉及 project IO / schema：

```bat
cmake --build build --config Release --target SimProject-V3IoSmokeTest
build\Release\bin\SimProject-V3IoSmokeTestrx64.exe
```

## 12. 推荐先做的最小切片

建议下一步先做一个小切片：

```text
新增 CollisionTargetRef / resolver
    -> 不改 schema
    -> 不改 UI 主结构
    -> 只集中“项目实体如何变成 collision target”的逻辑
```

这个切片风险最低，并且是后续懒构建与 GUI 收口共同需要的基础。
