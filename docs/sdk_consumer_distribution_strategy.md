# Core/Platform 预编译 SDK 使用与分发策略

## 问题背景

当前本地安装目录大致包含：

```text
cmake/
Common/
SMRobotCore/
SMRobotPlatform/
thirdparty/
SMRobotSDKExportSnapshot.txt
SMRobotSDKManifest.json
```

这个形态更接近一个“可独立消费的完整 SDK prefix”，而不是一个只替换源码树中 `SMRobotCore` 和 `SMRobotPlatform` 的最小二进制补丁包。

现在需要同时满足三件事：

1. `SMRobotCore` 和 `SMRobotPlatform` 的源码不交给下游，只交付 DLL/import lib/public headers/CMake package。
2. 其他还没成熟的功能仍然可以给下游以源码方式继续开发，成熟后再决定是否封装成 SDK。
3. 统一维护的 `thirdparty` 不希望每次 Core/Platform 小改动都重新复制、重新发布。

## 当前 install 包应该如何使用

当前 install 目录应该被当作一个 CMake package prefix 使用，而不是让下游手工 include 某些目录、手工 link 某些 `.lib`。

典型 consumer 工程写法：

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyRobotApplication LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotSDK)
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS ProjectSimulationSDK)

add_executable(MyRobotApplication main.cpp)
target_link_libraries(MyRobotApplication PRIVATE
    SMRobotCore::RobotSDK
    SMRobotPlatform::ProjectSimulationSDK
)
```

配置时：

```powershell
cmake -S path\to\consumer -B path\to\consumer\build -G "Visual Studio 16 2019" -DCMAKE_PREFIX_PATH=D:\path\to\SMRobotSDK
cmake --build path\to\consumer\build --config Release
```

下游不应该：

- 直接 include `SMRobotCore/Collision/include` 这类内部目录；
- 直接 link `Collision_shared_rx64.lib` 这类具体文件名；
- 直接依赖 `thirdparty/fcl_static_x64`；
- 复制 SDK 内部 CMake 片段到自己的工程；
- 修改 SDK 安装目录里的文件来解决功能缺口。

对于当前项目，已有 `tests/package_examples` 就是这种 consumer 模式的验证入口。后续 SDK 包中应该挑选一两个最小示例复制到 `examples/`，让下游从示例开始。

## 推荐交付形态

建议不要只有一种“全量胖 SDK”。应该拆成三类交付物。

### 1. Core/Platform Binary SDK

这是你要隐藏源码的部分。

内容：

- `SMRobotCore` public headers、DLL、import lib、CMake config。
- `SMRobotPlatform` public headers、DLL、import lib、CMake config。
- 必要的 `Common` 运行时/工具组件。
- SDK manifest、ABI snapshot、使用文档、consumer 示例。

用途：

- 下游源码项目通过 `find_package` 消费。
- 本仓库或业务仓库可以通过 `UsingPrebuilt_SMRobotCore=ON`、`UsingPrebuilt_SMRobotPlatform=ON` 做混合构建。

### 2. Business Source Package

这是还没成熟、还希望第三方继续开发的部分。

内容可以包括：

- 喷涂、打磨、搬运等业务模块源码；
- 应用层 UI/workbench；
- 工艺配置、模板、示例项目；
- 业务侧测试；
- 面向业务团队 AI 的 `AI_CONTEXT.md`。

它不包含 `SMRobotCore` 和 `SMRobotPlatform` 源码，而是通过 `find_package` 或预编译选项引用 Binary SDK。

推荐形态：

```text
BusinessSourcePackage/
  CMakeLists.txt
  apps/
  modules/
  examples/
  docs/
  thirdparty.local.props 或 sdk-path.cmake
```

### 3. ThirdParty SDK Cache

这是统一维护的第三方依赖。

内容：

- Boost、Qt、Assimp、KTX、VerificationSDK 等需要下游配置时解析或运行时加载的依赖；
- header-only 依赖，如 Eigen、glm、nlohmann_json；
- 版本 manifest；
- 编译器、运行时库、Debug/Release 约定。

它应该有自己的版本号和发布节奏，例如：

```text
SMRobotThirdParty-vs2019-x64-2026.07/
```

Core/Platform SDK 小改动时不应该重复复制整个 thirdparty。SDK manifest 只记录它依赖哪个 ThirdParty SDK 版本。

## 推荐目录关系

建议最终变成：

```text
SMRobotSDK-1.4.0/
  cmake/
  Common/
  SMRobotCore/
  SMRobotPlatform/
  docs/
  examples/
  SMRobotSDKManifest.json
  SMRobotSDKExportSnapshot.txt

SMRobotThirdParty-vs2019-x64-2026.07/
  Boost/
  Qt5/
  assimp/
  KTX/
  Eigen340/
  glm/
  nlohmann_json/
  Windows/VerificationDeveloperKit-1.0.0/
  manifest.json

BusinessSourcePackage-spray-dev/
  CMakeLists.txt
  SMRobotSpray/
  SMRobotApps/
  SimWorkbench/
  AI_CONTEXT.md
```

业务团队配置时传两个 prefix：

```powershell
cmake -S BusinessSourcePackage-spray-dev -B build\spray `
  -DCMAKE_PREFIX_PATH="D:\sdk\SMRobotSDK-1.4.0;D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07"
```

如果仍然使用当前主仓库做混合构建，则使用：

```powershell
cmake -S . -B build\business_prebuilt `
  -DUSER_TANGTANG_p15v3_Windows=ON `
  -DSMROBOT_PREBUILT_PACKAGE_ROOT=D:\sdk\SMRobotSDK-1.4.0 `
  -DUsingPrebuilt_SMRobotCore=ON `
  -DUsingPrebuilt_SMRobotPlatform=ON
```

## 为什么当前 thirdparty 会被复制

当前顶层 `CMakeLists.txt` 明确安装了若干 thirdparty 内容：

- `Eigen340`、`glm`、`nlohmann_json` 被安装到 `thirdparty/`，component 是 `smrobot_sdk_thirdparty_header_only`。
- `VerificationDeveloperKit` 被安装到 `thirdparty/Windows`。
- Boost 会被安装到 `thirdparty/Boost`。
- Assimp 等预编译依赖会被安装到 `thirdparty/assimp`。
- `vs2019/fcl_static_x64` 等目录会被安装到 `thirdparty/`，component 是 `smrobot_sdk_robotio_thirdparty`。

这说明当前 SDK profile 是“standalone profile”：目标是让 SDK 解压后尽可能自包含。它适合外部分发，但不适合内部高频迭代，因为每次 install 都复制大量 thirdparty，耗时很大。

## 推荐 thirdparty 策略

建议拆成两个 profile：

### Standalone Profile

用于正式对外或离线交付。

特点：

- SDK 包内包含必要 thirdparty；
- 下游只配置一个 SDK prefix；
- 包体大，安装慢；
- 适合不确定下游环境是否已有统一 thirdparty 的场景。

### Thin/Internal Profile

用于内部团队高频开发。

特点：

- SDK 包不复制统一 thirdparty；
- SDK manifest 记录 `ThirdParty SDK` 版本；
- 下游配置 `CMAKE_PREFIX_PATH` 时同时传入 Core/Platform SDK 和 ThirdParty SDK；
- 包体小，安装快；
- 更符合当前团队“thirdparty 已统一”的现状。

当前第一阶段建议先保留 standalone profile 能跑通，同时新增 thin/internal profile 的设计，不急着删除 standalone profile。

## 关于 fcl 是否应该导出

你的判断在设计目标上是对的：如果 `fcl` 只被 `SMRobotCore::Collision` 内部实现使用，并且 `Collision` 的 public API 不暴露任何 `fcl` 类型，那么它不应该作为第三方使用者需要感知的依赖导出。

但当前代码和 CMake 表达的事实不是这样。

### 当前事实

`SMRobotCore/Collision/TargetConfigSetting.cmake` 中：

```cmake
set( ${TARGET_NAME}_RequiredLibsPublic
    Eigen3::Eigen
    SMRobotCore::RobotCore
    fcl::fcl
)
```

这表示 `fcl::fcl` 是 `SMRobotCore::Collision` 的 public dependency。安装导出时，依赖生成逻辑会读取 `INTERFACE_LINK_LIBRARIES`，把 public 依赖写入 `CollisionDependencies.cmake`。所以下游如果直接 `find_package(SMRobotCore COMPONENTS Collision)`，CMake 会尝试解析 `fcl`。

同时，当前 `Collision` public headers 里确实暴露了 fcl：

- `Collision/CollisionGeometry.h` include `<fcl/fcl.h>`，并公开 `std::shared_ptr<fcl::CollisionGeometryd>`。
- `Collision/CollisionObject.h` include `<fcl/fcl.h>`，并公开 `std::shared_ptr<fcl::CollisionObjectd>` 和 `fcl()` getter。

所以对 `SMRobotCore::Collision` 这个 component 来说，`fcl` 目前不是完全私有实现细节，而是已经进入了 public header/API。

### 重要区别

如果第三方只使用 `SMRobotCore::RobotSDK`，那么 `RobotSDK` 的 public header 没有暴露 fcl，fcl 可以被视为 RobotSDK 背后的实现细节。

如果第三方直接使用 `SMRobotCore::Collision` component，那么当前 `Collision` public API 需要 fcl，fcl 就必须可解析。

因此问题不是“fcl 是否一定要发”，而是“我们是否允许第三方直接使用 `SMRobotCore::Collision` component”。

## fcl 的处理方案

### 方案 A：短期保守方案

保留 `fcl` 在 SDK 中，但明确它是 SDK 内部依赖，不是第三方可直接使用的 API。

做法：

- SDK 文档声明：外部 consumer 默认只使用 `RobotSDK`、`ProjectSimulationSDK` 等 facade。
- 不鼓励直接消费 `SMRobotCore::Collision`。
- `thirdparty/fcl_static_x64` 可以放在 standalone SDK 中，保证历史 component smoke 能跑。
- thin/internal profile 中可以不复制 fcl，但要求统一 ThirdParty SDK prefix 可解析 fcl。

优点：

- 改动最小；
- 不破坏当前 package examples；
- 适合第一阶段先跑通交付链路。

缺点：

- SDK 包里仍然能看到 fcl；
- `Collision` component 仍然暴露 fcl 事实；
- ABI/API 边界不够干净。

### 方案 B：中期正确方案

把 `fcl` 从 `Collision` public API 中移除。

目标：

- `Collision` public headers 不 include `<fcl/fcl.h>`。
- 不公开 `fcl::CollisionGeometryd`、`fcl::CollisionObjectd`。
- `CollisionGeometry`、`CollisionObject` 使用 PIMPL 或内部 handle 隐藏 fcl。
- `fcl::fcl` 从 `Collision` public link dependency 移到 private link dependency。
- installed `CollisionDependencies.cmake` 不再要求 consumer find `fcl`。

这才符合“fcl 被 `SMRobotCore::Collision` 完全封装”的目标。

风险：

- 这是 API/ABI 变化；
- 需要检查所有直接用 `CollisionObject::fcl()` 的内部调用；
- 需要迁移可能已经存在的 package example 或业务代码；
- 需要更新 ABI baseline。

### 方案 C：外部只发布 facade

不把 `SMRobotCore::Collision` 作为第三方正式 component 发布，只通过 `RobotSDK` 暴露碰撞能力。

做法：

- external SDK profile 只发布 `RobotSDK`、`ProjectSimulationSDK`、必要 runtime DLL。
- `Collision` DLL 可作为 runtime implementation dependency 随包带出，但不安装它的 public headers 和 CMake component target。
- 内部团队如果确实要直接用 `Collision`，使用 internal SDK profile。

优点：

- 对外 API 最干净；
- 不必立刻重构 Collision/fcl 边界；
- 最符合“底层复杂实现由 SDK facade 封装”的方向。

缺点：

- 内部高级用户不能直接调用 `Collision` 低层 API；
- 需要区分 external profile 和 internal profile。

## 其他未成熟功能如何给第三方开发

不要把这些功能强行塞进 Core/Platform SDK。建议使用“源码包 + 预编译依赖”的模式。

例如：

```text
SprayBusinessDev/
  CMakeLists.txt
  SMRobotSpray/                  # 源码
  SMRobotApps/                   # 只保留需要开发的应用源码
  SimWorkbench/                  # 只保留需要开发的 workbench 源码
  docs/
    AI_CONTEXT.md
    SDK_USAGE.md
```

它依赖：

```text
SMRobotSDK-1.4.0                 # 预编译 Core/Platform
SMRobotThirdParty-vs2019-x64     # 统一 thirdparty
```

这样做的好处：

- Core/Platform 源码不暴露；
- 业务团队仍然能正常改业务源码；
- 业务功能成熟后，可以再变成新的 SDK facade；
- 下游不会误以为所有源码都属于可稳定依赖的 API。

## 建议的下一步

第一步不要急着删源码做 `branch2`。先把交付形态定义清楚。

建议按以下顺序推进：

1. 定义 SDK profile：
   - `core_platform_standalone`：包含 thirdparty，适合离线完整交付。
   - `core_platform_thin_internal`：不复制统一 thirdparty，适合内部团队高频开发。
   - `core_platform_external_facade`：只发布稳定 facade，不发布低层 component headers。

2. 定义 public component 白名单：
   - 第一批建议：`RobotSDK`、`ProjectSimulationSDK`。
   - 需要可视化时再加入 `VisualizationSDK`。
   - `Collision`、`RenderCore`、`SceneCore`、`RobotRenderBridge` 暂时归为 internal component。

3. 明确 fcl 策略：
   - 短期：standalone profile 可以带 fcl，文档声明不允许业务直接使用。
   - 中期：重构 `Collision` public headers，移除 fcl 暴露。
   - 长期：external facade profile 不发布 `Collision` component。

4. 做一个业务源码包模板：
   - 删除或排除 `SMRobotCore`、`SMRobotPlatform` 源码。
   - 通过 `find_package` 或 `UsingPrebuilt_SMRobotCore/Platform` 使用 SDK。
   - 保留业务模块源码。
   - 附带 `AI_CONTEXT.md`，告诉第三方 AI 哪些能用、哪些不能用。

5. 建立版本关系：
   - `SMRobotSDK_VERSION`
   - `SMRobotThirdParty_VERSION`
   - `BusinessSourcePackage_VERSION`

## 推荐结论

当前 install 结构可以作为第一版内部验证用的 standalone SDK，但不建议直接把它作为最终分发形态。

更合理的方向是：

- Core/Platform 二进制 SDK 和业务源码包分开。
- thirdparty 单独版本化，不随每次 SDK 小改重复复制。
- fcl 短期作为内部依赖随 standalone 包存在；中期从 `Collision` public API 中移除；外部正式 SDK 尽量只通过 `RobotSDK` 暴露碰撞能力。
- 对第三方开发者和第三方 AI，只暴露 facade、示例和明确边界，不鼓励直接依赖底层 component。
