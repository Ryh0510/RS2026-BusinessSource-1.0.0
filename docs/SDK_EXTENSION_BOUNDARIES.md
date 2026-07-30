# SMRobotCore SDK Extension Boundaries

## Purpose

This document defines the release boundary between the core robot SDK,
future headless project SDKs, and future visualization SDKs.

The goal is to keep `RobotSDK` small, stable, and usable without OpenGL,
SceneCore, Qt, or application-level project viewers.

## Layer 1: Core RobotSDK

`RobotSDK` is the only primary SDK facade exported by the SMRobotCore package.
It is responsible for:

- loading URDF and Simscape robot models
- reading robot model metadata
- creating runtime robot instances
- setting joints and base transforms
- querying link transforms
- running headless robot/environment collision checks

Allowed implementation dependencies behind `RobotSDK`:

- `RobotCore`
- `RobotRuntime`
- `RobotIO`
- `RobotInstance`
- `Kinematics`
- `Collision`
- small utility/static dependencies required by those modules

Forbidden in the public `RobotSDK` interface:

- OpenGL types or handles
- SceneGraph, renderer, render pass, shader, texture, or model ownership
- Qt widgets, windows, signals, slots, or UI types
- direct public exposure of internal module classes

External users should include only `RobotSDK/*.h` and link only
`SMRobotCore::RobotSDK`.

## Layer 2: Future ProjectSimulationSDK

A future headless project SDK may wrap project documents and project-level
simulation workflows. It should remain separate from `RobotSDK`.

Candidate responsibilities:

- load and validate `.scene.json` or project documents
- resolve project assets
- build project runtime state without a renderer
- run project-level collision checks
- expose project robot/environment state through SDK-owned DTOs

Allowed dependencies:

- `SimulationProject`
- `SimulationRuntime`
- `RobotSDK` or the same headless robot/collision internals used by `RobotSDK`

Forbidden dependencies:

- `RenderCore`
- `SceneCore`
- `RobotRenderBridge`
- Qt/Application viewer code

Do not add project document concepts to `RobotSDK` unless they are needed by
most robot-only users and can be represented without project-specific types.

### 当前阶段 C 状态

当前已经新增第一版 `SMRobotPlatform::ProjectSimulationSDK`，定位为项目读取 facade，而不是运行时控制 facade。

当前 public header 只暴露 SDK 自有 DTO、`Result`、`IProjectSimulationSession` 和 factory 函数；不 include `SimulationProject`、`SimulationRuntime`、Qt、SceneCore、RenderCore、OpenGL 或 Eigen。

当前能力范围：

- 创建项目 SDK session。
- 读取项目文件。
- 校验项目文档。
- 返回项目摘要。
- 枚举项目实体信息。

当前刻意不暴露：

- runtime 构建和运行控制。
- `ProjectSimulationRuntime` object。
- `SimulationProject::ProjectDocument` 原始类型。
- 渲染、场景、Qt viewer 或 Workbench 类型。

原因：第一版仍是静态 facade，如果直接链接 `SimulationRuntime`，外部 consumer 会被迫解析 `RobotIO`、`AssetCore`、`SensorSimulation`、OpenGL/Assimp 等实现闭包，不符合 SDK 发布边界。运行时能力应等 facade 稳定后通过更窄 API 或 DLL facade 引入。

## Layer 3: Future RobotVisualizationSDK

A future visualization SDK may wrap scene and rendering workflows. It should
remain separate from both `RobotSDK` and the headless project SDK.

Candidate responsibilities:

- create visual robot scene nodes
- convert robot/collision/project state into `SceneCore` objects
- manage renderer-facing visualization configuration
- support GLFW/Qt viewer integration through adapter code

Expected dependencies:

- `SceneCore`
- `RenderCore`
- `RobotRenderBridge`
- `CameraCore`
- optionally `ProjectSimulationSDK` for project state input

Forbidden in `RobotSDK`:

- any dependency on this visualization SDK
- any API requiring a graphics context
- any public OpenGL resource lifetime ownership

## Current Package Decision

The current package intentionally exports:

- `SMRobotCore::RobotSDK`
- `SMRobotCore::RobotTrajectoryCore` as a compatibility component

It intentionally does not export:

- `SimulationProject`
- `SimulationRuntime`
- `RobotRenderBridge`
- `SceneCore`
- `RenderCore`
- `CameraCore`
- `SMRobotApps/RobotQtViewer`

阶段 C 后，`SMRobotPlatform::ProjectSimulationSDK` 可以作为独立项目级 SDK facade 导出；但 `SimulationProject` 和 `SimulationRuntime` 仍不建议作为第三方主入口直接发布。

These modules may be built from source inside the repository, but they are not
part of the public SMRobotCore SDK package surface.

## Criteria For Adding A New SDK Target

Before adding a new public SDK target, verify:

- the target has a clear audience and lifecycle independent from `RobotSDK`
- public headers do not expose internal implementation classes
- exported CMake dependencies do not force unrelated layers
- examples consume the target through `find_package`
- install layout includes docs and a minimal consumer example
- ABI/version policy is documented before release

If these criteria are not met, keep the target internal.
