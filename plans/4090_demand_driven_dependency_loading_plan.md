# 4090 用户配置按需加载第三方依赖方案

## 1. 状态与结论

- 当前状态：4090 保守试点已实施并完成首轮配置、构建验证。
- 默认行为：`RS2026_4090_ENABLE_DEMAND_LOADING=OFF`，仍加载原有 provider；观察报告默认开启。
- 试点范围：`USER_TANGTANG_4090_Windows`、Visual Studio 2019、x64。
- 总体可行性：高。
- 包级按需加载可行性：高。全项目依赖汇总发生在 `include(UserConfig)` 之前，现有 `AllPackagesRequired` 已可作为判断输入。
- Boost component 精确加载可行性：高。`CMake_FindBoost1.78.0.cmake` 已支持调用方通过 `Boost_Components` 指定组件。
- 一次性把所有 finder 全自动化的可行性：中等。在处理生成器表达式、复合 finder、传递依赖和可选示例依赖之前，不建议直接全面替换。

## 2. 目标

1. 4090 用户配置只加载当前构建图真正需要的第三方 package。
2. 对支持 components 的 package，只加载当前构建图需要的 components。
3. 用户配置只负责“本机到哪里找、使用哪个版本”，不再重复维护“项目业务上依赖什么”。
4. 保持 Source、Prebuilt、Business Source 三种构建模式一致。
5. 所有按需判断都来自一个规范化后的全项目依赖快照，避免各处自行判断。
6. finder 被跳过时输出明确原因；finder 被执行后校验所需 imported targets 确实存在。

## 3. 非目标

1. 本轮试点不同时重写其他用户配置。
2. 不迁移第三方库版本，不更换 Boost 1.78、Qt 5.15.5、OMPL 2.0.1 等版本。
3. 不修改第三方库本身的 CMake package。
4. 不把 Qt、OMPL 或其他外部依赖加入 Core/Platform SDK。
5. 不通过全局 `include_directories()`、`link_directories()` 修补缺失依赖。
6. 不在本轮试点中修改其他用户配置或默认启用按需加载。

## 4. 当前链路

当前配置顺序已经具备按需加载的基础：

```text
扫描源码/预编译 package
  -> 读取各 PackageConfigSetting.cmake
  -> 汇总各 TargetConfigSetting.cmake 的 RequiredLibsPublic/Private
  -> ProjectRequiredLibs
  -> resolve_dependencies(ProjectRequiredLibs)
  -> AllPackagesRequired + <Package>_DepLibs
  -> include(UserConfig)
  -> include(TANGTANG_4090_Windows.cmake)
  -> 当前无条件加载全部固定 finder
```

附件中的汇总结果已经证明，工程可以得到类似以下信息：

```text
AllPackagesRequired = Boost;OpenGL;FreeGLUT;glm;Eigen3;Qt5;ompl;...
Boost_DepLibs = headers;timer;filesystem;locale;log;log_setup;date_time
Qt5_DepLibs = Widgets;OpenGL
```

因此问题不在于“无法知道需要什么”，而在于“4090 用户配置没有消费这个结果”。

## 5. 当前必须先解决的问题

### 5.1 生成器表达式被当成 package 名

当前 `resolve_dependencies()` 直接按 `::` 拆分字符串，导致：

```text
$<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
```

被统计为：

```text
package = $<BUILD_INTERFACE:nlohmann_json
component = nlohmann_json>
```

这会产生伪 package，不能作为可靠的按需加载输入。

处理原则：

- `TargetConfigSetting.cmake` 是依赖元数据，不应复制 `target_link_libraries()` 中的生成器表达式。
- 优先把元数据中的依赖写成规范 target 名，如 `nlohmann_json::nlohmann_json`。
- 汇总器仍应增加输入校验和有限的 `BUILD_INTERFACE`/`LINK_ONLY` 解包能力，防止以后再次污染。
- `INSTALL_INTERFACE` 依赖不进入源码构建的用户配置快照。

### 5.2 直接依赖不等于有效依赖闭包

项目直接依赖 `ompl::ompl`，而 OMPL 2.0.1 的 package config 还要求：

```text
Boost::serialization
Eigen3::Eigen
Threads::Threads
```

附件中的直接汇总没有 `Boost::serialization`。如果 4090 配置只把 `Boost_DepLibs` 原样交给 Boost finder，OMPL 的传递需求仍可能在后续查找时失败。

试点策略：

- 区分“项目直接 components”和“provider 有效 components”。
- 当 `ompl` 被需要时，在 4090 provider 层为 Boost 有效组件补充 `serialization`，并记录原因为 `ompl 2.0.1 transitive requirement`。
- 最终传给 Boost finder 的是去重后的有效集合。
- 长期应尽量让标准 CMake package 自己解析传递依赖；本机 provider 只负责使其依赖路径可被找到。

### 5.3 复合 finder 不能简单一对一判断

当前存在一个 finder 同时提供多个 package/target 的情况：

- `CMake_FindFCL` 同时加载 `ccd`、`octomap`、`fcl`。
- `Findpinocchio_3rdParty` 同时加载 `console_bridge`、`urdfdom_headers`、`urdfdom`、`pinocchio`。
- `FindCoACD_3rdParty` 提供 `CoACD::_coacd`、`CoACD::coacd` 和运行时复制函数。

这类 finder 应按“所提供 target 集合中任意一个被需要”判断，而不是只检查文件名对应的单个 package。

### 5.4 可选示例依赖没有进入主 package 汇总

GLFW 主要出现在 diagnostics、feature probes 和 external validation 中。当前全项目主依赖汇总只读取 package/component 的 `TargetConfigSetting.cmake`，不会自动读取这些可选目标的 `target_link_libraries()`。

如果直接根据主汇总跳过 GLFW，随后启用 feature probe 时可能缺失 `glfw::glfw`。

试点必须采用以下二选一策略中的第一种：

1. 第一阶段保留可选类别显式条件：当相关 `BuildDiagnostics*`、`BuildFeatureProbes*`、`BuildExternalValidation*` 为 ON 时加载 GLFW。
2. 后续为每个可选类别增加轻量依赖元数据，再统一并入依赖快照。

不建议在配置阶段解析任意 `CMakeLists.txt` 中的 `target_link_libraries()` 文本。

### 5.5 Source 与 Prebuilt 的依赖范围不同

- Source package 需要其 public 和 private 构建依赖。
- Prebuilt package 不应重新加载已封装的 private 构建依赖，只应由安装包 config 解析公开依赖。
- 当前 `ProjectPackagesConfigSetting.cmake` 只把 source package 的依赖加入 `ProjectRequiredLibs`，这个方向是正确的，应保留。
- 4090 判断必须使用最终全项目快照，不能重新扫描被 `UsingPrebuilt_*` 替代的源码依赖。

## 6. 建议的中央查询接口

在 `cmake/FuncDef_GetPackageDeps.cmake` 中扩展现有能力，形成最小中央 API：

```cmake
rs_project_requires_package(<package> <out_bool>)
rs_project_required_components(<package> <out_list>)
rs_project_requires_any_package(<out_bool> <package>...)
rs_assert_required_targets(<provider_name> <target>...)
```

建议同时生成只读语义的变量：

```text
RS2026_REQUIRED_PACKAGES
RS2026_REQUIRED_COMPONENTS_Boost
RS2026_REQUIRED_COMPONENTS_Qt5
...
```

兼容期内继续同步现有变量：

```text
AllPackagesRequired
Boost_DepLibs
Qt5_DepLibs
...
```

约束：

- package 与 component 去重但保持首次出现顺序，方便日志审计。
- 每次 configure 都重新计算，不使用可能残留的 cache 组件列表。
- 非法依赖项产生明确 warning 或 fatal error，不能静默生成伪 package。
- 查询函数不执行 `find_package()`，只回答当前构建图需要什么。

## 7. 4090 试点写法

第一阶段不引入复杂的通用 provider 注册框架。4090 文件保留清晰的显式块，但每一块都通过中央查询接口决定是否加载：

```cmake
rs_project_requires_package(Boost _need_boost_direct)
rs_project_requires_package(ompl _need_ompl)

if(_need_boost_direct OR _need_ompl)
    rs_project_required_components(Boost Boost_Components)
    if(_need_ompl)
        list(APPEND Boost_Components serialization)
    endif()
    list(REMOVE_DUPLICATES Boost_Components)
    include(CMake_FindBoost1.78.0)
else()
    message(STATUS "[DependencyProvider] Skip Boost: not required by current build graph.")
endif()
```

其他 package 使用相同模式：

```cmake
rs_project_requires_package(Qt5 _need_qt5)
if(_need_qt5)
    rs_project_required_components(Qt5 Qt5_Components)
    include(CMake_FindQt5155)
endif()
```

对不支持 components 的 finder，只做 package 级按需加载并验证目标：

```cmake
rs_project_requires_package(assimp _need_assimp)
if(_need_assimp)
    include(CMake_FindAssimp6.0.2)
    rs_assert_required_targets(assimp assimp::assimp)
endif()
```

## 8. 4090 provider 映射建议

| 汇总 package/触发条件 | 4090 provider | components 处理 | 备注 |
|---|---|---|---|
| `glm` | `Findglm_3rdParty` | 单 target | 需要时加载 `glm::glm` |
| `Eigen3` 或 OMPL/FCL 传递需求 | `FindEigen_3rdParty` | 单 target | 需要时加载 `Eigen3::Eigen` |
| `nlohmann_json` | `Findnlohmann_json_3rdParty` | 单 target | 先修复生成器表达式污染 |
| `FreeGLUT` | `Findfreeglut_3rdParty` | 当前只需 `freeglut` | 检查 `FreeGLUT::freeglut` |
| `glfw` 或启用相关可选类别 | `Findglfw_3rdParty` | 单 target | 不能只看主依赖快照 |
| `Boost` 或 `ompl` | `CMake_FindBoost1.78.0` | 精确传入 `Boost_Components` | OMPL 补充 `serialization` |
| `assimp` | `CMake_FindAssimp6.0.2` | 无细分 component | 检查 `assimp::assimp` |
| `fcl`、`ccd`、`octomap` 任一 | `CMake_FindFCL` | 复合 provider | 只调用一次 |
| `ompl` | `CMake_FindOMPL` | 当前 `ompl` | 必须在 Boost/Eigen 可解析后加载 |
| `KTX` | `CMake_FindKtx` | 当前 `ktx` | 检查 `KTX::ktx` |
| `Qt5` | `CMake_FindQt5155` | 传入 `Qt5_DepLibs` | 预期为 `Widgets;OpenGL` 等实际集合 |
| `OpenGL` | CMake 内置 `find_package(OpenGL)` | 传入实际组件 | 检查 `OpenGL::GL` |
| `urdfdom*`、`console_bridge`、`pinocchio` 任一 | 拆分后的 URDF/pinocchio provider，过渡期可复用 `Findpinocchio_3rdParty` | 复合 provider | 当前 finder 过度加载 pinocchio，建议后续拆分 |
| `CoACD` 或 Collision 显式可选能力 | `FindCoACD_3rdParty` | 单 provider、多附加变量 | 当前依赖没有完整进入 TargetConfig 元数据 |
| `OpenSSL` | `CMake_FindOpenSSL3.6.1` | 仅真实 target 依赖时加载 | 当前正式依赖快照未见 OpenSSL target |

## 9. Boost component 策略

### 9.1 直接组件

直接从规范化后的 `Boost_DepLibs` 获取，例如：

```text
headers;timer;filesystem;locale;log;log_setup;date_time
```

### 9.2 传递组件

由当前实际启用的 provider 补充：

```text
ompl 2.0.1 -> serialization
```

### 9.3 最终有效集合

```text
Boost_EffectiveComponents
  = Boost_DirectComponents
  + Boost_ProviderComponents
```

在调用 finder 前去重，并输出来源报告：

```text
[DependencyProvider] Boost direct components: ...
[DependencyProvider] Boost provider components: serialization (required by ompl 2.0.1)
[DependencyProvider] Boost effective components: ...
```

### 9.4 finder 约束

- 必须在 include 前显式 `set(Boost_Components ...)`，避免 finder 回退到默认全组件集合。
- Debug/Release、动态/静态策略继续由 4090 配置和 Boost finder 管理。
- `Boost::log`、`Boost::log_setup` 保持动态链接策略。
- finder 完成后逐个检查有效 component 对应的 `Boost::<component>` target。
- 重复 configure 时不得沿用上一次构建图留下的组件列表。

## 10. 修改范围与实施阶段

### 阶段 A：清理依赖快照

预计文件：

- `cmake/FuncDef_GetPackageDeps.cmake`
- `SMRobotMotionPlanning/MotionPlanningCore/TargetConfigSetting.cmake`
- `SMRobotCore/RobotIO/TargetConfigSetting.cmake`
- `SMRobotPlatform/SimulationProject/TargetConfigSetting.cmake`

工作内容：

1. 规范化依赖项并校验 `Namespace::Component` 格式。
2. 修复 `nlohmann_json` 的 `BUILD_INTERFACE` 伪 package。
3. 建立中央查询函数和稳定的只读依赖快照。
4. 保留现有变量作为兼容输出。

### 阶段 B：4090 包级按需加载

预计文件：

- `cmake/UserConfigs/TANGTANG_4090_Windows.cmake`

工作内容：

1. 给每个现有 finder 增加基于依赖快照的显式条件。
2. 对复合 finder 使用 any-package 判断。
3. 对跳过和加载都输出统一日志。
4. finder 后验证所需 imported targets。
5. 暂不改动其他用户配置。

### 阶段 C：Boost 精确 components

预计文件：

- `cmake/UserConfigs/TANGTANG_4090_Windows.cmake`
- 如确有必要，再调整 4090 使用的 Boost finder；优先不修改 `D:/PreBuild` 中的共享文件。

工作内容：

1. 将 `Boost_DepLibs` 复制为本轮直接组件。
2. 合并 OMPL 等 provider 的传递组件。
3. 精确设置 `Boost_Components` 后调用现有 finder。
4. 验证不会加载 finder 默认的 Python、program_options、random 等无关组件。

### 阶段 D：可选目标依赖闭包

预计文件：

- `cmake/ProjectPackagesConfigSetting.cmake`
- `cmake/FuncDef_ExampleCategory.cmake`
- 相关 package 的可选类别依赖元数据文件；仅在确有需要时增加。

工作内容：

1. 把示例类别开关定义提前到用户配置判断之前。
2. 第一版用类别开关保护 GLFW 等可选 provider。
3. 后续再将可选目标依赖纳入统一快照。

### 阶段 E：验证后推广

只有 4090 试点通过后，才把相同查询 API 迁移到其他 Windows/Ubuntu 用户配置。各用户文件继续负责本机路径和版本，不复制依赖业务规则。

## 11. 验证矩阵

### 11.1 静态验证

1. `AllPackagesRequired` 不再出现 `$<BUILD_INTERFACE:...` 伪 package。
2. 所有 package component 去重且名称合法。
3. 4090 配置中不存在无条件第三方 finder，基础路径设置除外。
4. PowerShell/CMake 格式和 `git diff --check` 通过。

### 11.2 配置场景

1. 完整源码默认构建：需要的 Assimp、FCL、KTX、Boost、Qt 等全部加载。
2. Business Source：Prebuilt Common/Core/Platform 的私有依赖不被重新加载。
3. 关闭 MotionPlanning：OMPL finder 被跳过，OMPL 引入的 Boost serialization 不进入有效集合。
4. 关闭 Qt 应用/Workbench：Qt finder 被跳过。
5. 仅启用 Core 的最小配置：不加载 Qt、OMPL、Assimp、KTX、GLFW。
6. 启用 GLFW feature probe：即使主依赖不含 GLFW，也能通过可选类别条件加载。
7. 连续在同一 build 目录切换上述开关，确认组件列表不会残留。

### 11.3 Boost 专项

1. 日志打印直接、传递、最终三组 components。
2. 实际创建的 `Boost::*` target 覆盖最终集合。
3. 不需要的默认 components 不被加载。
4. OMPL 构建与链接通过，`Boost::serialization` 可用。
5. `CustomLog` 源码模式仍使用动态 Boost.Log；Prebuilt CustomLog 不触发 Boost 开发包查找。

### 11.4 构建验证

1. 4090 VS2019 x64 CMake configure。
2. SDK Debug/Release 目标与 consumer matrix。
3. Business Source Release `RobotQtViewer`。
4. 至少一个 GLFW feature probe。
5. MotionPlanning/OMPL 目标。

## 12. 风险与停止条件

1. 如果某个 target 在 `TargetConfigSetting.cmake` 和实际 `target_link_libraries()` 中的依赖不一致，先修复元数据，不在用户配置中增加永久例外。
2. 如果一个 finder 提供多个无关 package，优先拆分 provider；过渡期可用 any-package 条件，但要记录拆分条件。
3. 如果第三方 package 的传递依赖只能依赖 finder 的隐式副作用，停止进一步收紧并先补充 provider 依赖说明。
4. 如果启用可选目标后出现未登记依赖，不恢复全部无条件加载；补充该类别的依赖元数据或明确条件。
5. 试点阶段不得修改其他用户配置，以便与原行为对照。
6. 不允许通过本机绝对路径写入 target 的公开 interface。

## 13. 完成标准

- 4090 用户配置的每个第三方 finder 都有明确触发条件。
- 当前构建不需要的 package 不执行 finder，也不要求其安装目录存在。
- Boost 只加载直接组件与已声明的传递组件。
- Source、Prebuilt、Business Source 三种模式均能正确配置。
- 可选示例开启时不会因为主依赖快照遗漏而失败。
- 日志能够回答：为什么加载、加载哪些 components、由哪个 target/provider 引入。
- 4090 试点通过后，再决定是否抽象为通用 provider registry 并推广到其他用户。

## 14. 首轮实施记录

已完成：

1. 修正三处 `nlohmann_json` 依赖元数据，不再把 `BUILD_INTERFACE` 生成器表达式统计为伪 package。
2. `resolve_dependencies()` 现在生成规范化只读快照，并提供 package/component 查询与 target 校验函数。
3. 4090 配置新增以下开关：
   - `RS2026_4090_ENABLE_DEMAND_LOADING`：默认 `OFF`，控制是否真正按需跳过 provider。
   - `RS2026_4090_DEPENDENCY_REPORT`：默认 `ON`，输出实际动作、按需动作及原因。
4. Boost 在按需模式下加载直接 components，并合并 OMPL、Pinocchio 的传递 components。
5. Source 和 Prebuilt 使用不同触发来源：源码依赖来自全项目快照，预编译 Core/Platform 的 FCL、URDF、Assimp、Eigen、glm 需求由实际请求的预编译 components 补充。
6. VS 调试运行目录只收集当前实际存在的 imported targets，允许 Qt、Assimp、URDF 等 provider 被安全跳过。
7. CoACD 依赖元数据尚不完整，继续采用保守加载；Qt finder 仍由其现有实现加载固定组件集合，本轮只控制是否加载 Qt package。

验证结果：

1. 默认兼容模式完整源码 CMake 配置通过。
2. 4090 按需模式完整源码 CMake 配置通过，GLFW、OpenSSL 被跳过，Boost 有效集合为 9 个 components。
3. 4090 按需模式完整源码 `RobotQtViewer` Release 构建通过。
4. Business Source 模拟场景（Prebuilt Core + Platform）CMake 配置通过，预编译组件能正确触发 FCL、URDF、Assimp 等 provider。
5. Business Source 模拟场景 `RobotQtViewer` Release 构建通过。
6. Common + Core 最小场景配置通过，并实际跳过 GLFW、Assimp、OMPL、KTX、Qt5、OpenSSL。

当前仍不建议把按需开关改为默认 `ON`。建议先在日常 4090 构建中观察一段时间，再处理 CoACD 元数据和 Qt components 精确化。
