# 碰撞检测系统概览

本文说明当前碰撞检测系统的数据结构、运行时管理方式、碰撞分类逻辑，以及项目文件如何保存和加载这些配置。

## 1. 总体分层

当前碰撞功能可以分成三层：

- 项目保存层：由 `SimulationProject` 中的 JSON 数据结构负责。
- 运行时碰撞层：由 `collision::CollisionScene` 及相关结构负责。
- Scene 可视化层：由 `SceneCore` 节点和碰撞调试 overlay 负责显示。

需要注意的是：碰撞数据并不是直接保存在 `SceneCore::SceneGraph` 里的。`SceneCore` 负责显示机器人、工件和调试绘制结果；真正参与 FCL 检测的运行时碰撞对象由 `collision::CollisionScene` 管理。

## 2. 项目保存层的数据结构

可保存到项目 JSON 的数据结构定义在：

- `SMRobotPlatform/SimulationProject/include/SimulationProject/ProjectDocument.h`

主要结构如下。

### `simulation_project::ProjectDocument`

这是整个场景工程的根结构，包含：

- `robots`
- `objects`
- `collision`
- `collisionVisualization`
- `collisionDetectors`
- `robotCollisionOverrides`
- `view`

也就是说，机器人、场景物体、碰撞检测器、碰撞体覆盖配置和视图信息都从这里保存和恢复。

### `simulation_project::CollisionQueryDesc`

这是旧版碰撞配置结构。

它包含：

- `pairs`：机器人和机器人之间的碰撞 pair。
- `robotObjectPairs`：机器人和场景物体之间的碰撞 pair。

如果项目里还没有新版 `collisionDetectors`，运行时会根据这个旧结构生成一个默认 detector。

### `simulation_project::CollisionDetectorDesc`

这是新版“碰撞检测器”的项目描述结构。

它保存：

- detector 的 `id`、`name`、`type`
- 是否启用
- `targets`
- `pairGenerators`
- `pairFilters`
- `excludePairs`
- `geometryRole`
- contacts、nearest points、distance 等查询选项
- visualization 配置

一个 `CollisionDetectorDesc` 可以理解为：一次独立的碰撞检测配置。比如全场景检测、机器人自碰撞检测、两个指定连杆之间的检测等。

### `simulation_project::CollisionPairGeneratorDesc`

这个结构描述“如何生成碰撞对象 pair”。

目前支持的类型包括：

- `SceneAll`
- `RobotSelf`
- `RobotRobot`
- `RobotObject`
- `RobotObjectGroup`
- `ObjectObject`
- `LinkLink`
- `LinkRobot`
- `LinkObject`
- object group 相关组合

例如 `LinkLink` 会只取两个指定连杆下的 collision object；`RobotRobot` 会取两个机器人的 collision object 并做笛卡尔积。

### `simulation_project::RobotCollisionOverrideDesc`

这个结构保存用户编辑出来的机器人碰撞体覆盖配置。

它可以：

- 内嵌保存在 `.scene.json` 项目文件里。
- 保存成单独的 `*.collision.override.json` sidecar 文件，再由项目文件记录 `overridePath`。

### `simulation_project::CollisionElementOverrideDesc`

这个结构表示一个用户编辑出来的碰撞元素，例如 box、sphere、mesh 等。

它包含：

- `id`
- `linkName`
- `label`
- `type`
- `role`
- `enabled`
- `localTransform`
- `boxSize`
- `radius`
- `length`
- `meshPath`
- `meshScale`
- `inflationMargin`
- `source`

它的粒度是“某个 link 上的一个 collision element”。

## 3. 运行时碰撞层的数据结构

运行时碰撞核心结构定义在：

- `SMRobotCore/Collision/include/Collision/CollisionScene.h`
- `SMRobotCore/Collision/include/Collision/RobotCollisionModel.h`
- `SMRobotCore/Collision/include/Collision/RobotCollisionInstance.h`
- `SMRobotCore/Collision/include/Collision/CollisionQueryOptions.h`
- `SMRobotCore/Collision/include/Collision/CollisionResult.h`

### `collision::CollisionScene`

`CollisionScene` 是当前场景中真正负责碰撞检测的运行时容器。

它内部主要保存：

- `world_`
  - 底层碰撞世界。
  - 当前实现最终会走 FCL backend。

- `robots_`
  - `RobotCollisionInstancePtr` 列表。
  - 每个元素代表一个机器人实例的碰撞对象集合。

- `environment_`
  - 环境或场景物体的 `CollisionObjectPtr` 列表。

- `environmentInfo_`
  - 环境对象的元信息。
  - 用于在检测结果中把某个 `ObjectID` 解释为工件、环境物体等。

- `acm_`
  - `AllowedCollisionMatrix`。
  - 用来屏蔽允许碰撞或不需要检测的 object pair。

`CollisionScene` 不是渲染结构，它不拥有 OpenGL 资源，也不属于 `SceneCore`。

### `collision::RobotCollisionModel`

`RobotCollisionModel` 是机器人级别的碰撞模型。

其中 `RobotCollisionElement` 表示一个连杆上的一个碰撞几何，包含：

- `linkName`
- `elementName`
- `geometry`
- `shapeDesc`
- `localTransform`
- `role`
- `inflationMargin`
- `group`
- `mask`
- `label`

现在的设计不是“一个 link 只能有一个碰撞体”，而是一个 link 可以有多个 `RobotCollisionElement`。这对 sphere cover、planning proxy、simplified geometry、安全边界和用户手工添加 primitive 都很重要。

### `collision::RobotCollisionInstance`

`RobotCollisionInstance` 是 `RobotCollisionModel` 的运行时实例。

它会为每个 link element 构建一个 `CollisionObject`。这个对象的 `ObjectID` 由以下信息 hash 得到：

- robot instance id
- link name
- element name

重要映射包括：

- `objects_`
  - 从 `linkName:elementName` 映射到 `CollisionObjectPtr`。

- `objectsByLink_`
  - 从 link name 映射到该 link 下所有 collision object key。

- `idToLink_`
  - 从 `ObjectID` 反查到 `LinkInfo`。
  - `LinkInfo` 包含机器人实例 id、link name、element name 和 geometry role。

这套反查机制很关键：FCL 只知道两个 object 碰了，但应用层需要知道是哪个机器人、哪个连杆、哪个碰撞元素发生了碰撞。

## 4. 碰撞如何分类

当前分类不是通过复杂继承体系完成的，而是通过两部分信息完成：

- 项目层的 detector 类型和 pair generator 类型。
- 运行时 object 的元信息。

### 项目层分类

`CollisionDetectorDesc::type` 和 `CollisionPairGeneratorDesc::type` 描述了检测器想检测什么。

常见分类包括：

- `SceneAll`：全场景检测。
- `RobotSelf`：机器人自碰撞检测。
- `RobotRobot`：机器人和机器人检测。
- `RobotObject`：机器人和场景物体检测。
- `ObjectObject`：场景物体之间检测。
- `LinkLink`：两个指定连杆之间检测。
- `LinkRobot`：某个连杆和某个机器人检测。
- `LinkObject`：某个连杆和某个物体检测。

### 运行时 pair 生成

`SMRobotApps/RobotQtViewer/ProjectCollisionDetectorRuntime.cpp` 中的 `ProjectCollisionDetectorBuilder` 会把项目里的 detector 描述转换成 `collision::CollisionQueryOptions`。

`CollisionQueryOptions` 中关键字段包括：

- `scope`
- `geometryRole`
- `selectedObjects`
- `includePairs`
- `excludePairs`

几个典型例子：

- `RobotRobot`
  - 收集 robot A 的碰撞对象。
  - 收集 robot B 的碰撞对象。
  - 两组对象做笛卡尔积，生成 `includePairs`。

- `LinkLink`
  - 只收集指定 link A 下的碰撞对象。
  - 只收集指定 link B 下的碰撞对象。
  - 两组对象生成 pair。

- `RobotSelf`
  - 收集同一个机器人内的碰撞对象。
  - 对这些对象做两两组合。

- `RobotObject`
  - 收集机器人 link objects。
  - 收集场景 object 的 collision object。
  - 两组对象生成 pair。

- `SceneAll`
  - 不依赖显式 pair 列表，而是让 scope 走全场景检测。

### 检测结果分类

检测结果中的语义信息保存在 `collision::CollisionObjectInfo` 中。

机器人对象一般满足：

- `robotInstance >= 0`
- `linkName` 是机器人连杆名
- `elementName` 是碰撞元素名

环境或场景对象一般满足：

- `robotInstance == -1`
- `linkName` 通常是 `object` 或 `environment`
- `elementName` 标识具体场景物体

`CollisionScene::fillResultInfo()` 会在碰撞检测后，把 FCL 返回的 `ObjectID` 反查成这些语义信息。

## 5. 碰撞结构如何进入 ProjectScene

应用层入口主要在：

- `SMRobotApps/RobotQtViewer/ProjectScene.cpp`

`ProjectScene::Impl` 同时持有视觉场景和碰撞场景：

- `scenecore::SceneGraph sceneGraph`
- `collision::CollisionScene collisionScene`
- `std::vector<RuntimeRobot> robots`
- `std::vector<RuntimeSceneObject> objects`
- `std::vector<ProjectCollisionDetectorRuntime> collisionDetectors`

### 初始化流程

1. `ensureProjectDocument()`
   - 如果外部没有设置 project document，则加载默认项目。

2. `buildProjectRobots()`
   - 解析机器人路径。
   - 加载机器人模型。
   - 应用 robot collision override。
   - 构建机器人可视化 bridge。
   - 构建 robot collision model 和 collision instance。
   - 调用 `collisionScene.addRobot(...)` 加入碰撞场景。

3. `buildProjectObjects()`
   - 加载场景物体模型。
   - 创建 `SceneCore` 可视化节点。
   - 从 mesh 构建 triangle mesh 碰撞几何。
   - 调用 `collisionScene.addEnvironmentObject(...)` 加入碰撞场景。

4. `applyProjectCollisionAndView()`
   - 将保存的 `collisionDetectors` 转换成运行时 detectors。
   - 选择 active detector。
   - 应用碰撞可视化设置。

### 每帧更新流程

每一帧大致流程是：

1. 更新机器人运动学。
2. 同步机器人 link transform 到 collision objects。
3. 调用 `collisionScene.update()` 更新底层碰撞世界。
4. active detector 调用 `collisionScene.checkCollision(...)`。
5. 根据 `CollisionResult` 更新机器人 mesh overlay 和场景物体高亮。
6. 调用 `collisionScene.buildDebugDraw(...)` 生成碰撞调试绘制数据。
7. 通过 `robot_render::CollisionRenderBridge::draw(...)` 把 overlay 提交给渲染器。

因此，`SceneCore` 只负责显示；碰撞检测本身仍然在 `Collision` 模块中完成。

## 6. 保存和加载

项目保存和加载由：

- `SMRobotPlatform/SimulationProject/src/ProjectIo.cpp`

中的两个函数完成：

- `simulation_project::loadProjectDocument(...)`
- `simulation_project::saveProjectDocument(...)`

项目 JSON 会保存：

- robots
- scene objects
- legacy collision query
- collision visualization
- collision detectors
- robot collision overrides
- view

### 保存路径

RobotQtViewer 中的保存入口是：

- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `MainWindow::saveProjectToPath(...)`

保存时会先整理机器人、场景物体和 sidecar override 的资源路径，使其尽量可移植。然后调用 `saveProjectDocument(...)` 写出 JSON。

### 加载路径

加载流程是：

1. `MainWindow` 调用 `loadProjectDocument(...)` 读取 JSON。
2. 读取结果赋给 `m_projectDocument`。
3. viewport reload project。
4. `ProjectScene` 根据 document 重新构建 runtime robots、objects、collision scene 和 detectors。

所以项目文件里保存的是“描述”；真正的 runtime collision objects 是每次打开项目时重新构建出来的。

## 7. Inline Override 和 Sidecar Override

机器人碰撞体编辑结果有两种保存方式：

- 内嵌在 `.scene.json` 项目文件中。
- 保存为独立的 `*.collision.override.json` sidecar 文件。

内嵌方式存放在：

- `ProjectDocument::robotCollisionOverrides`

sidecar 方式由以下字段引用：

- `RobotCollisionOverrideDesc::overridePath`

项目加载时，`RobotCollisionOverrideApplier` 会检查是否存在 `overridePath`。如果有，它会加载 sidecar 文件并应用到机器人模型；如果没有，就使用项目文件内嵌的 override 数据。

这意味着：只要用户保存了项目，或者保存了 sidecar 并让项目文件记录 sidecar 路径，编辑出来的碰撞几何就可以在下次打开软件时恢复。

## 8. 关键模块边界

当前碰撞系统的模块边界是比较清晰的：

- `Collision`
  - 负责碰撞对象、查询、结果、geometry role 和 ACM。

- `SimulationProject`
  - 负责可序列化的项目描述。

- `RobotIO`
  - 负责从 URDF 或其他来源提供机器人碰撞几何。

- `RobotRenderBridge` / `SceneCore`
  - 负责把碰撞几何和碰撞结果可视化。

- `SceneCore`
  - 不拥有机器人碰撞语义。

一句话总结：项目 JSON 保存描述，`ProjectScene` 根据描述重建运行时碰撞对象，`CollisionScene` 执行检测，`SceneCore` 只负责显示视觉场景和碰撞 overlay。
