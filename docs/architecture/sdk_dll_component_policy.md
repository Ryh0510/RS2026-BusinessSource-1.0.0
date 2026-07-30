# SDK DLL Component 依赖与 ABI 策略

日期：2026-07-13

## 目标

本文档固化 Core/Platform prebuilt SDK 的 component 迁移规则。目标是让第三方以类似 Boost 的方式使用少量 package 和多个 component：

```cmake
find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotCore Collision)
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS AssetCore)
target_link_libraries(app PRIVATE SMRobotCore::Collision SMRobotPlatform::AssetCore)
```

每个 component 优先以 DLL 形式交付，除非该 component 明确是 header-only、object-like internal helper，或处于尚未迁移阶段。

## Package/component 命名规则

- package 名称保持稳定，例如 `SMRobotCore`、`SMRobotPlatform`。
- component 名称保持与现有 CMake target 一致，例如 `RobotCore`、`Collision`、`AssetCore`。
- installed target 名称保持 namespaced 形式，例如 `SMRobotCore::Collision`、`SMRobotPlatform::AssetCore`。
- 不通过 E 阶段迁移重命名 public C++ API。

## Public/private dependency 判定

### Public dependency

满足以下任一条件时，该依赖应放入 component 的 public dependency：

- public header 直接 `#include` 了该依赖的 header，且 consumer 编译 public header 时需要它。
- public class、struct、enum、函数签名、模板参数、返回值或成员类型暴露了该依赖的类型。
- installed import target 的 `INTERFACE_LINK_LIBRARIES` 必须包含该依赖，否则 consumer 链接或编译无法通过。

### Private dependency

满足以下条件时，该依赖应放入 private dependency：

- 只在 `.cpp` 或 private header 中使用。
- 只用于实现细节、日志、文件解析、运行时加载或内部算法。
- consumer 使用 public header 和 import library 时不需要直接查找该依赖 target。

### 当前迁移例子

- `SMRobotCore::Collision` public header 暴露 `fcl/fcl.h`，因此 `fcl::fcl` 目前是 public dependency。
- `SMRobotPlatform::AssetCore` public header 暴露 `Eigen` 和 `glm`，因此 `Eigen3::Eigen`、`glm::glm` 是 public dependency。
- `SMRobotPlatform::AssetCore` 只在 `.cpp` 使用 Assimp，因此 `assimp::assimp` 是 private dependency。

## Installed dependency alias 规则

installed SDK 的 dependency resolver 需要容忍部分第三方 package 的 target 命名差异。例如：

- source tree 内部可能使用 `fcl::fcl`；
- installed FCL config 可能只提供 `fcl` target。

规则：

- resolver 先按 public dependency target 名称判断目标是否已存在。
- 对 `X::X` 这类 namespace 与 component 同名的依赖，执行 `find_dependency(X REQUIRED)`。
- 如果 `find_dependency` 后只存在 `X` 而不存在 `X::X`，则补一个 alias target。
- 该规则只用于 installed package consumer 的兼容，不改变 source tree 的链接表达。

## DLL 输出规则

迁移为 DLL 的 component 应满足：

- `add_library(<component> SHARED ...)`。
- Debug/Release postfix 使用 `_shared_`。
- Windows 下安装 runtime DLL 到 `<Package>/bin`，import library 到 `<Package>/lib`。
- installed CMake target 和 dependency 文件随 component install。
- consumer smoke 应覆盖至少一次外部 `find_package(... COMPONENTS <component>)`、链接和运行。

## Export/ABI 策略

当前 E 阶段是 component 迁移期，不把“全部显式 export header”作为每个 component 迁移的前置条件。

阶段性规则：

1. 迁移期允许使用 `WINDOWS_EXPORT_ALL_SYMBOLS`，以降低 static 到 DLL 的首次切换风险。
2. 已有稳定 export header 的 component 可以继续使用现有宏，例如 `CameraCore`。
3. 不在一个普通 component 迁移中顺手批量改写所有 public class 的 export 宏。
4. 在 SDK ABI 冻结前，应为对第三方承诺的 DLL component 建立显式 export header，并明确：
   - build DLL 时 dllexport；
   - consumer 使用 DLL 时 dllimport；
   - 如保留 static variant，提供 static define；
   - 不导出内部实现类。
5. 显式 export header 的引入应按 component 独立评审和验证，避免一次性跨模块 ABI 改写。

## Consumer matrix 要求

每迁移一个普通 component，至少补充：

- 外部 package example。
- CTest 入口。
- component selection 检查：
  - 未请求时不应意外导入；
  - 显式请求时必须导入；
  - private dependency 不应被 public dependency 文件强制暴露。

## 后续迁移顺序建议

推荐先迁移依赖闭包较清晰、非重渲染链路的 component，再进入渲染/场景桥接链路：

1. `AssetCore`
2. `CameraCore` ABI/export 规则复核
3. `SensorCore`
4. 评估 `SceneCore` 与 `RenderCore` 的先后关系
5. 最后处理 `RobotRenderBridge`、`SimulationProject`、`SimulationRuntime` 等跨 Core/Platform 的聚合 component
