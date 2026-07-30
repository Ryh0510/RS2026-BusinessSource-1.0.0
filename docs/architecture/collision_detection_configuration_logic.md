# 碰撞检测配置设计逻辑说明

本文整理当前碰撞检测配置的真实语义、代码分层和后续推荐收口方向。它面向后续修改 `CollisionConfigMode`、碰撞检测运行时、碰撞几何代理和 GUI 配置面板时阅读。

核心分层如下：

```text
Collision Geometry
    负责“每个实体用什么碰撞形状”

Collision Target
    负责“哪些项目实体可以变成 ObjectID”

Collision Detector
    负责“哪些 target 之间生成 pair 并执行查询”
```

GUI 应该只暴露一种主要配置心智：

```text
Targets / Sets -> Detector
```

也就是先定义目标或目标集合，再定义检测器如何在集合内部或集合之间生成碰撞对。运行结果属于路径规划、轨迹规划、实时仿真、数字环境等运行/分析模块，不作为 `CollisionConfigMode` 的右侧配置页面。`targets`、`selectionSets`、`pairGenerators` 目前同时存在，是当前配置心智混乱的主要来源。

## 1. 最底层碰撞库的真实输入

底层碰撞库位于 `SMRobotCore/Collision`。它不认识项目树、GUI、selection set 或 detector 面板。

它主要认识：

- `CollisionGeometry`：碰撞几何，内部持有 FCL geometry。
- `CollisionObject`：一个可参与碰撞的对象，包含 `ObjectID`、geometry、transform 和 FCL object。
- `CollisionScene`：碰撞世界，负责添加 robot / environment object，执行 `checkCollision(...)` / `distance(...)`。
- `CollisionQueryOptions`：一次查询的参数，尤其是 `includePairs`、`excludePairs`、`selectedObjects`、`geometryRole`、`geometrySource`。
- `CollisionResult`：查询结果，包含 collision flag、contacts、nearest point、min distance 等。

底层最朴素的数据流是：

```text
CollisionGeometry
    -> CollisionObject(ObjectID, geometry, transform)
    -> CollisionScene
    -> CollisionQueryOptions
    -> CollisionResult
```

如果只检测两个物体，真实含义就是：

```text
ObjectID A + ObjectID B
    -> CollisionQueryOptions.includePairs = {(A, B)}
    -> CollisionScene::checkCollision(...)
```

如果检测多个物体，核心问题变成：如何生成一批 `ObjectID` pair。

## 2. 机器人为什么特殊

机器人不是单个 `CollisionObject`。机器人 collision model 是：

```text
RobotCollisionModel
    linkName -> collision elements
```

运行时 `RobotCollisionInstance` 会把每个 link 的每个 collision element 转成一个 `CollisionObject`。因此一个 link 可能展开成多个 `ObjectID`。

GUI 里选择一个 robot link 时，底层真实含义是：

```text
robotId + linkName
    -> RuntimeCollisionRobot
    -> RobotCollisionInstance
    -> linkName 对应的一个或多个 CollisionObject / ObjectID
```

这也是 project 文件中保存 `robotId + linkName`，而不是保存 `ObjectID` 的原因。`ObjectID` 属于运行态，不是稳定项目 ID。

## 3. 三层配置语义

### Collision Geometry

这一层回答“每个实体用什么碰撞形状”。

相关数据包括：

- `RobotCollisionOverrideDesc`
- `ObjectCollisionOverrideDesc`
- `ObjectCollisionElementOverrideDesc`
- detector 上的 `geometryRole`
- detector 上的 `geometrySource`

常见问题包括：

```text
使用原始 collision？
使用视觉 mesh？
使用自动生成 proxy？
使用 Exact / Simplified / SafetyMargin / SphereCover / PlanningProxy？
是否替换原始 collision？
```

### Collision Target

这一层回答“哪些项目实体可以参与碰撞检测”。

相关数据包括：

- `CollisionSelectionSetDesc`
- `CollisionSelectionSetMemberDesc`
- `CollisionDetectorTargetDesc`

目标可以指向：

- robot
- robot link
- scene object
- object group
- mounted attachment
- point cloud，当前沿用 object id 风格引用

这一层不应该关心最终执行算法，只负责稳定表达“用户选了哪些项目实体”。

### Collision Detector

这一层回答“哪些 target 之间生成 pair 并执行查询”。

相关数据包括：

- `CollisionDetectorDesc`
- `CollisionPairGeneratorDesc`
- `CollisionPairFilterDesc`
- `excludePairs`
- `queryPolicy`
- `selectionSetAId`
- `selectionSetBId`

这一层把 target 展开成底层 `ObjectID`，再生成 `includePairs` / `excludePairs`，最后驱动 `CollisionScene` 查询。

## 4. 当前三种检测对象表达方式

当前 `CollisionDetectorDesc` 同时支持三种描述方式。

### selectionSets + queryPolicy

用户先维护目标集合：

```text
Set A = robot1.link3, robot1.link4
Set B = objectA, objectB
```

detector 再定义：

```text
queryPolicy = BetweenSets
selectionSetAId = A
selectionSetBId = B
```

运行时：

```text
Set A members -> ObjectID list A
Set B members -> ObjectID list B
A x B -> includePairs
```

这是未来推荐的 GUI 主线，因为用户心智清晰：集合 A 的所有对象，与集合 B 的所有对象，两两一一匹配。

### targets

detector 直接保存目标：

```text
targets:
    robotId + includeLinks / excludeLinks
    objectId
    objectGroupId
```

这种方式适合程序生成或高级配置，但不适合作为 GUI 主心智，因为它和 selection set 的职责重复。

### pairGenerators

detector 内部直接保存 pair 生成规则：

```text
LinkLink
LinkRobot
LinkObject
RobotRobot
RobotObject
ObjectObject
ObjectGroupObjectGroup
```

这是最接近底层 pair 生成的表达，适合快捷命令或高级模式。例如用户在两个 link 上右键直接创建一个 LinkLink detector。

## 5. 推荐收口方向

未来 GUI 配置入口应以 `Targets / Sets -> Detector` 为主线。

推荐解释：

```text
Targets
    可被碰撞检测引用的项目实体。

Sets
    用户维护的 target 集合。

Detector
    定义集合内部或集合之间如何生成 pair，并定义查询参数。

Runtime Result / Overlay
    由运行/分析模块显示 detector 的运行结果和 overlay，不进入 CollisionConfigMode 配置面板。
```

这意味着：

- `selectionSets + queryPolicy` 应成为 GUI 主路径。
- `targets` 可保留为 detector 内部兼容或程序生成路径，但不作为主要用户入口。
- `pairGenerators` 可保留为高级快捷路径，例如“从两个 link 创建 detector”，但 UI 应把它解释为“创建了一个包含明确 pair rule 的 detector”，而不是第三套配置主线。
- `CollisionQueryDesc::robotObjectPairs` 应逐步标记为 legacy / migration 路径。

## 6. 懒构建碰撞对象的目标模型

后续应强化一个性能设计原则：

```text
没有被启用 detector 查询引用的实体，不应该构建 collision object。
```

目标流程：

```text
ProjectDocument::collision.detectors
    -> 过滤 enabled detector
    -> 收集 detector 引用的 CollisionTarget
    -> 解析 CollisionTarget 到 robot/link/object/attachment/pointCloud
    -> 按 geometryRole / geometrySource 收集所需 CollisionGeometryRequest
    -> 构建最小 RuntimeCollisionRobot / RuntimeCollisionObject 集合
    -> 生成 includePairs / excludePairs
    -> 执行 CollisionScene query
```

需要注意：

- robot link 只有被 detector 引用时才需要构建其 collision element。
- 如果 detector 使用 `AllEnabled`，才需要扩大到所有可碰撞对象。
- 如果 detector 使用 `BetweenSets`，只需要构建 set A / set B 直接或间接引用的对象。
- 如果 detector 使用 `LinkLink` pair generator，只需要构建这两个 link 的 collision object。
- geometry role/source 也应参与需求收集；例如只查询 `PlanningProxy` 时，不必构建 `Exact` geometry。

## 7. 推荐术语

后续建议统一使用以下术语：

- `Collision Geometry`：碰撞形状与代理。
- `Collision Target`：可参与检测的项目实体引用。
- `Collision Set`：用户维护的一组 collision targets。
- `Collision Detector`：生成 pair 并执行查询的配置。
- `Collision Pair`：两个底层 `ObjectID` 的检测组合。
- `Collision Result`：runtime 输出结果。
- `Collision Overlay`：结果或几何在 viewport 中的显示。

不要把 geometry override、target set、detector、runtime overlay 都笼统叫“collision config”。如果必须用总称，应明确上下文。
