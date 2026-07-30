# 碰撞运行时性能分析与后续修改建议

## 当前结论

本轮只分析，不修改源码。

当前 `RobotQtViewer` 的主循环由 `RobotViewport` 的 `QTimer` 驱动，约每 16ms 请求一次 `update()`，随后在 `paintGL()` 中调用 `ProjectScene::update()` 和 `ProjectScene::render()`。

碰撞系统分为两层：

- `collisionScene.update()`：当前每帧都会执行，用于更新 FCL broadphase/world，日志中约 `0.3ms`，不是当前卡顿主因。
- detector 查询：只有 `detector.enabled == true` 时才执行 `checkCollision()` / `distance()`。

用户提供的日志显示当前主要瓶颈是 `distance()`：

- `checkMs` 约 `19ms-31ms`。
- `distanceMs` 有时达到 `2189ms-3482ms`。
- `worldUpdateMs` 约 `0.3ms`。

因此当前秒级卡顿来自最近距离/最近点查询，而不是 broadphase world update。

## GUI 中已有开关

当前 GUI 已经有 detector 级别开关：

- `Collision` 工作台
- `Detectors` 页
- `Detector List` 中每个 detector 前面的勾选框
- 或选中 detector 后，属性区中的 `Enabled` 复选框

这两个入口最终都会影响：

```cpp
if(detector.enabled)
```

相关代码：

- `CollisionDetectorsWidget` 中 `Detector List` 项的 check state 发出 `detectorEnabledChanged`。
- `CollisionDetectorsWidget` 属性区中存在 `Enabled` 复选框。
- `CollisionDetectorCommandController::setDetectorEnabled()` 写入文档并调用 viewport。
- `ProjectScene::setCollisionDetectorEnabled()` 修改 runtime detector 的 `enabled`。

GUI 中也已有 `Nearest / distance` 复选框。该开关会影响：

- `detector->nearestPoints`
- `detector->distance`
- runtime 中的 `options.enableNearestPoints`
- runtime 中的 `options.enableDistance`

当前代码中 `detector->distance = detector->nearestPoints`，所以 GUI 里 `Nearest / distance` 实际是一个合并开关。

## nearest/distance 为什么不是每帧都有

这不是 FCL 随机行为，而是 Viewer 代码主动调度的结果。

`ProjectScene::update()` 中每帧都会先做 `checkCollision()`，但 `distance()` 只有在 `shouldRefreshNearest == true` 时执行。

当前逻辑大致是：

- 第一次没有结果时执行。
- 上一帧发生碰撞而当前需要恢复 nearest 状态时执行。
- 没有可复用的 nearest 结果时执行。
- active detector 每 4 帧刷新一次。
- 非 active 但 visible detector 每 15 帧刷新一次。

如果本帧不刷新 nearest，且上一帧已有 nearest 结果，则直接复用旧结果。

因此日志会出现：

- 多数帧 `distanceMs=0`，总耗时约 `20ms`。
- 某些帧触发 `distance()`，总耗时突然变成 `2s-3.5s`。

## 295 对 includePairs 的原因判断

日志中：

```text
includePairs=295
effectivePairs=295
```

这说明运行时并不是只处理一个高层语义 pair，而是展开到了 295 个底层 FCL object pair。

结合当前代码和用户说明，原因大概率是：

- Object 使用三角网格碰撞模型。
- 机器人 Link6/工具也使用三角网格碰撞模型，且非凸、表面复杂。
- 当前碰撞模型会把 visual/collision mesh 构建为 FCL `BVHModel<fcl::OBBRSSd>`。
- 如果一个 link 或一个 object 有多个 collision element / submesh / override element，那么一个“Link6 vs Object”的语义检测会展开成多个底层 object pair。

相关代码路径：

- `ProjectRuntimeBuilder::buildSceneObject()` 会把 Object mesh 转为 `CollisionShapeType::TriangleMesh`。
- `CollisionGeometryBuilder::buildTriangleMesh()` 会创建 `fcl::BVHModel<fcl::OBBRSSd>`。
- `FclCollisionBackend::checkCollision()` / `computeDistance()` 会遍历所有 object pair，并用 `shouldQueryPair()` 过滤。

因此，295 不是 FCL 自动把一个 mesh 拆成 295 个 pair；更准确地说，它是项目运行时把多个 collision object 注册到了 collision world，detector 又把这些 object 组合成了 295 个有效 pair。每个单独的三角网格 object 内部由 FCL BVH 处理。

## 为什么 mesh distance 极慢

FCL 的三角网格 mesh-mesh 碰撞 boolean 查询通常还能接受，但 mesh-mesh 最近距离/最近点查询非常昂贵，尤其在 Debug、非凸复杂曲面、pair 数量较多时。

当前代码中已经有一个针对 mesh-mesh contact 的保护：

```cpp
// FCL mesh-mesh contact traversal can stall on large STL pairs; keep exact boolean detection.
```

这说明项目之前已经遇到过 mesh-mesh contacts 可能卡住的问题，并对 contacts 做了 fallback。但 `distance()` 路径目前仍直接调用：

```cpp
fcl::distance(...)
```

所以 nearest/distance 在复杂 mesh 上仍然可能产生秒级耗时。

## 修改建议

### 建议 1：默认关闭复杂 mesh detector 的 Nearest / distance

对三角网格精确模型，默认只做 boolean collision，不默认做 nearest/distance。

好处：

- 立刻消除 `distanceMs=2s-3.5s` 的卡顿。
- 保留是否碰撞的核心判断。

代价：

- 不再实时显示最近点和最近距离。

### 建议 2：交互期间暂停 nearest/distance

在拖动 Object、编辑 Frame、调整机器人姿态时，临时关闭 nearest/distance，只保留可选的 boolean collision。

Apply 或用户停止交互后，再延迟刷新一次 nearest。

### 建议 3：把 nearest/distance 改成手动刷新

在 GUI 中保留 `Nearest / distance`，但不自动每 4 帧刷新。改为：

- 显示上一次结果。
- 用户点击 `Refresh Distance` 时计算一次。
- 或设置很低频率，例如 1s/2s 一次，并且不阻塞交互。

### 建议 4：增加 detector 运行模式

建议新增三种模式：

- `Collision Only`：只查碰撞，推荐默认。
- `Collision + Contacts`：查碰撞和接触点。
- `Manual Nearest`：最近点/距离只手动刷新。

### 建议 5：检查 295 pair 的来源

需要增加一个诊断视图或日志，列出 detector 展开后的 pair：

- pair A object id
- pair A robot/link/element
- pair B object id
- pair B object/link/element
- shape type
- triangle count 或 proxy type

这样可以判断 295 是否合理，是否存在 detector 配置覆盖过大、重复注册、或 object/link 被过度拆分。

### 建议 6：使用简化碰撞代理

对复杂 Link6/工具和筒状 Object，长期建议使用：

- primitive
- simplified mesh
- sphere cover
- planning proxy
- safety margin proxy

精确三角网格适合离线验证，不适合作为 Debug 实时 nearest/distance 查询默认模型。

## 推荐优先级

1. 先把复杂 mesh detector 的 `Nearest / distance` 默认关闭或交互时禁用。
2. 再增加 `Refresh Distance` 手动按钮。
3. 增加 pair 展开诊断日志，确认 295 的组成。
4. 最后再考虑 FCL 层面的 mesh distance fallback 或 proxy 自动选择。

