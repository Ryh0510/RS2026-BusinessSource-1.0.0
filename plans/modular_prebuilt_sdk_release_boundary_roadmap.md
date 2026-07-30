# 模块化 Prebuilt SDK 发布边界路线图

## 1. 文档目的

本文记录模块化开发、prebuilt package、选择性 DLL 化和第三方扩展交付的长期路线。它用于在当前阶段完成后继续推进下一阶段，避免每次重新讨论时丢失判断依据。

本文只制定路线和阶段 B 的详细执行计划，不要求立即修改源码、CMake 或 public API。

## 2. 当前判断

当前平台已经证明 `install/prebuilt` 主链路可用。阶段 A 的 M0-M6 已完成：源码基线、install、外部 consumer、Core prebuilt、Core+Platform prebuilt、Workbench prebuilt、源码缺席验证均已通过。

这说明以下目标已经具备基础：

1. 基础包可以通过 install prefix 被外部工程消费。
2. 上层源码可以链接下层 prebuilt package。
3. 部分 Workbench package 已经可以在源码缺席时通过 prebuilt 形式参与 `RobotQtViewer` 构建。
4. 后续可以继续推进“部分源码隐藏、部分包开放、第三方基于 SDK 开发”的发布形态。

但当前还不能直接宣称已经形成正式第三方 SDK。主要缺口是：

1. 多数 target 仍是 `STATIC`，只有少数 facade 或基础库是 `SHARED`。
2. installed headers 仍可能暴露内部类、Qt widget、controller 或 render/scene 细节。
3. Workbench package 可 prebuilt 消费，但尚未全部成为长期稳定 API 边界。
4. `RobotQtViewer` 对关键 Workbench 仍偏编译期组合，不是运行时插件系统。
5. 当前可以“不给源码也能构建”，但还没有完全做到“只暴露稳定 facade，其余实现隐藏”。

因此，下一步不是立即大规模 DLL 化，而是先进入阶段 B：发布边界清单。

## 3. 长期路线

### 阶段 A：Prebuilt 可用性验证

状态：已完成。

目标是证明 install prefix 可以在缺少部分源码时被可靠消费。该阶段已经完成，是后续 SDK 化和 DLL 化的前置门槛。

### 阶段 B：发布边界清单

状态：下一步优先执行。

目标是整理所有 package、component、target、headers 的发布属性，明确哪些是 public SDK facade，哪些是 extension API，哪些只是内部实现，哪些可以安装但不承诺长期 ABI。

这是当前最应该马上做的事情。

### 阶段 C：Facade SDK 收敛

目标是在不暴露内部实现的前提下，为第三方提供稳定入口。优先考虑：

1. `SMRobotCore::RobotSDK`：继续作为机器人基础能力 facade。
2. `ProjectSimulationSDK`：项目文件、资产解析、项目运行时、项目级碰撞和仿真任务 facade。
3. `VisualizationSDK`：scene/render/robot visualization contribution facade。
4. `WorkbenchExtensionSDK`：Workbench 注册、action、panel、tree projection、viewport overlay、文档命令接入 facade。

第三方应依赖这些 facade，而不是直接依赖 `RobotCore`、`RobotIO`、`SceneCore`、`RenderCore`、Qt 内部 widget 或 viewer controller。

### 阶段 D：SDK 头文件闭包

目标是让外部开发者只 include 稳定头文件，不被迫 include 内部目录。

本阶段需要检查每个已安装 target 的 public headers：

1. 是否包含私有路径。
2. 是否暴露内部实现类。
3. 是否暴露 Qt widget、OpenGL handle、SceneGraph 或 RenderCore 细节。
4. 是否把 implementation dependency 变成 public dependency。
5. 是否可以通过 DTO、handle、interface 或 facade 减少外泄。

### 阶段 E：Package Profile

目标是形成不同交付包：

1. `CoreSDK`：机器人基础能力。
2. `SimulationSDK`：项目级仿真能力。
3. `VisualizationSDK`：可视化贡献能力。
4. `WorkbenchExtensionSDK`：Qt 工作台扩展能力。
5. `SpraySDK`、`WeldingSDK`、`HandlingSDK` 等业务扩展包。
6. `InternalFullDevPackage`：内部完整开发包。

不同第三方可以拿到不同 profile，而不是一律拿到全部源码或全部 headers。

### 阶段 F：选择性 DLL 化

目标是只把稳定 ABI 的 facade 或成熟业务包改成 DLL。

推荐顺序：

1. 完善既有 `SMRobotCore::RobotSDK` shared library。
2. 待 `ProjectSimulationSDK` facade 稳定后改为 DLL。
3. 待 `VisualizationSDK` facade 稳定后改为 DLL。
4. Workbench 先保持 prebuilt package 形态，等 contribution API 稳定后再决定是否 DLL 化。
5. 内部算法、IO、scene、render、collision target 默认继续作为 facade 背后的实现，不优先直接 DLL 化。

不建议把 `SMRobotCore` 或 `SMRobotPlatform` 的所有小 target 一次性改为 DLL。这样会过早暴露 ABI、符号导出、Debug/Release runtime、生命周期和依赖边界问题。

### 阶段 G：运行时插件化

目标是在编译期 package 扩展成熟后，再设计 runtime plugin loader。

本阶段需要单独定义：

1. plugin manifest。
2. C ABI 或稳定 C++ ABI 入口。
3. 版本检查和能力声明。
4. 依赖路径和 runtime DLL 部署。
5. 加载失败隔离。
6. QObject/Qt 生命周期。
7. 是否支持卸载，以及卸载时资源释放规则。

运行时插件化不是当前阶段目标。

## 4. 当前“马上要做的第一件事”

马上要做的第一件事就是阶段 B：发布边界清单。

原因：

1. 阶段 A 已证明 prebuilt 可行，下一步应决定“哪些东西可以作为包给别人”。
2. 没有发布边界清单，直接 DLL 化会把内部实现误固化为 ABI。
3. 第三方开发需要的是稳定 facade 和 extension API，不是仓库内部所有 target。
4. Package profile、头文件闭包、DLL 化顺序都依赖阶段 B 的分类结果。

阶段 B 本身应先作为审计和设计任务执行，不修改源码。

## 5. 阶段 B 详细计划

### 5.1 阶段目标

阶段 B 的目标是产出一份可执行的发布边界清单，回答以下问题：

1. 当前有哪些 package、component、target 会被 install/export。
2. 哪些 target 是第三方可直接依赖的 public SDK facade。
3. 哪些 target 是 Workbench/UI extension API。
4. 哪些 target 是业务 Domain API。
5. 哪些 target 只是内部实现，不能作为长期 SDK 暴露。
6. 哪些 target 暂时可安装但只能标为 compatibility。
7. 哪些 target 未来适合 DLL 化，哪些不适合。
8. 每个 package 对外应安装哪些 headers，隐藏哪些 headers。

### 5.2 非目标

阶段 B 不做以下事情：

1. 不修改 C++ 源码。
2. 不修改 CMake install/export 规则。
3. 不把任何 target 改成 `SHARED`。
4. 不新增 runtime plugin loader。
5. 不重命名 public target。
6. 不移动目录。
7. 不承诺当前所有 installed headers 都是长期 SDK API。

### 5.3 输入文档

继续推进发布边界时优先读取：

1. `docs/architecture/modular_simulation_platform_sdk_contract.md`
2. `docs/architecture/sdk_dll_component_policy.md`
3. `docs/architecture/workbench_gui_domain_package_separation.md`
4. `docs/architecture/sim_workbench_common_boundary.md`
5. `docs/SDK_API.md`
6. `docs/SDK_EXTENSION_BOUNDARIES.md`
7. `docs/SDK_PACKAGING.md`
8. `docs/sdk_abi_export_policy.md`
9. `docs/agent_change_audit.md`

### 5.4 需要审计的源码和配置范围

阶段 B 只读取和记录，不修改。

重点审计：

1. 根 `CMakeLists.txt`
2. `cmake/FuncDef_PackageInstall.cmake`
3. `cmake/ProjectPackagesConfigSetting.cmake`
4. `cmake/SMRobotDependencyBootstrap.cmake`
5. 各一级包的 `PackageConfigSetting.cmake`
6. `Common`
7. `SMRobotCore`
8. `SMRobotPlatform`
9. `SMRobotApps`
10. `SimWorkbench`
11. `SMRobotSpray`
12. `tests/package_examples`

### 5.5 产物文件建议

阶段 B 建议新增或更新以下文档：

1. `docs/sdk_abi_export_policy.md`
   - 当前导出面和 ABI 约束。
2. `docs/architecture/modular_simulation_platform_sdk_contract.md`
   - 仅在需要沉淀长期准则时更新。
3. `docs/agent_change_audit.md`
   - 记录本阶段审计结果和后续建议。

如果只做计划，不进入执行，则只保留本文即可。

### 5.6 Target 分类规则

每个 target 至少归入以下一种主分类。

| 分类 | 含义 | 对外策略 |
| --- | --- | --- |
| Public SDK Facade | 稳定 SDK 门面，第三方可直接依赖 | 可安装 headers/libs，可考虑 DLL |
| Extension API | 第三方扩展需要的接口层 | 可安装 headers/libs，先稳定 API 再考虑 DLL |
| Workbench UI Package | Qt 工作台实现或 UI 组件 | 可 prebuilt，未必承诺 ABI |
| Domain API | 喷涂、焊接、搬运、规划等业务算法/API | 按业务包策略选择开放 |
| Internal Implementation | facade 背后的内部实现 | 不作为第三方直接依赖 |
| Compatibility | 为旧包、旧工程、旧 target 名保留 | 保留原因和移除条件必须写清 |
| App Shell | exe 或 viewer composition root | 通常不作为 SDK API |

### 5.7 每个 target 的记录字段

清单中每个 target 建议记录：

| 字段 | 说明 |
| --- | --- |
| Package | 所属一级包，例如 `SMRobotCore` |
| Target | CMake target 名称 |
| Namespace Target | 例如 `SMRobotCore::RobotSDK` |
| Target Type | `STATIC` / `SHARED` / `INTERFACE` / `EXECUTABLE` |
| 当前是否 install | 是/否 |
| 当前是否 export | 是/否 |
| 当前是否 prebuilt 验证过 | 是/否/部分 |
| 建议分类 | Public SDK Facade / Internal 等 |
| 建议发布 profile | CoreSDK / SimulationSDK / WorkbenchExtensionSDK 等 |
| Public headers | 当前对外头文件范围 |
| Header 风险 | 是否暴露内部类型、Qt、OpenGL、Scene/Render 细节 |
| Public dependencies | 对外传播的依赖 |
| DLL 候选级别 | P0/P1/P2/不建议 |
| 保留或收敛建议 | 下一阶段动作 |

### 5.8 DLL 候选分级

DLL 候选不按“库有多大”决定，而按 API 稳定性和发布价值决定。

| 级别 | 含义 | 示例判断 |
| --- | --- | --- |
| P0 | 已是 facade，API 较稳定，优先保持或完善 DLL | `RobotSDK` |
| P1 | 应先设计 facade，稳定后 DLL | `ProjectSimulationSDK`、`VisualizationSDK` |
| P2 | 可 prebuilt，但暂不急于 DLL | 业务 Workbench 实现包 |
| 不建议 | 内部实现，不应直接形成对外 ABI | `RobotIO`、`SceneCore`、`RenderCore` 等内部 target |

### 5.9 阶段 B 执行步骤

#### B0：固定当前基线

记录：

1. 当前分支。
2. `git status --short`。
3. 当前阶段 A 的最后通过 build/install prefix。
4. 当前硬件认证相关修改是否已纳入主线判断。

本步骤只记录，不改变文件。

#### B1：枚举所有 package

从根 CMake、`PrebuiltPackages` 机制、一级目录和 `PackageConfigSetting.cmake` 枚举当前 package。

输出 package 清单：

1. `Common`
2. `SMRobotCore`
3. `SMRobotPlatform`
4. `SMRobotApps`
5. `SMRobotWorkbenchCommon`
6. `SMRobotWorkbenchProjectAssembly`
7. `SMRobotWorkbenchCollisionConfig`
8. `SMRobotWorkbenchRobotRun`
9. `SMRobotWorkbenchMotionPlanning`
10. `SMRobotSpray`
11. 其他已存在或未来预留业务包

#### B2：枚举所有 exported targets

从 CMake 文件和 install 目录中交叉检查：

1. `add_library`
2. `add_executable`
3. `function_InstallTarget`
4. `install(EXPORT ...)`
5. `*Targets.cmake`
6. `find_package(... COMPONENTS ...)`

输出 target 清单，避免只凭源码目录推断。

#### B3：检查 headers 暴露面

对每个 exported target 检查：

1. install include 目录。
2. public header 是否 include private header。
3. public header 是否暴露 Qt widget、OpenGL、SceneGraph、RenderCore、controller 或具体实现类。
4. 是否需要改成 DTO、handle、interface 或 facade。

本步骤先记录风险，不修改 headers。

#### B4：检查 public dependencies

对每个 exported target 检查：

1. `target_link_libraries(... PUBLIC ...)`
2. `INTERFACE_LINK_LIBRARIES`
3. generated `*Dependencies.cmake`
4. 第三方依赖是否通过 SDK bootstrap 闭包解析。

目标是确认外部 consumer 依赖的是 package facade，而不是被迫知道所有内部 target。

#### B5：制定 SDK profile 草案

基于 B1-B4 结果，给出第一版发布 profile：

1. `CoreSDK`
2. `SimulationSDK`
3. `VisualizationSDK`
4. `WorkbenchExtensionSDK`
5. `SprayExtensionSDK`
6. `InternalFullDevPackage`

每个 profile 明确：

1. 包含哪些 package。
2. 包含哪些 targets。
3. 包含哪些 headers。
4. 需要哪些 runtime files。
5. 第三方能否基于该 profile 开发源码扩展。

#### B6：制定 DLL 顺序草案

基于 target 分类和 header 风险，给出 DLL 化排序：

1. 保持或完善 `RobotSDK`。
2. 先设计 `ProjectSimulationSDK`，不要直接 DLL 化 `SimulationProject` 内部组件。
3. 先设计 `VisualizationSDK`，不要直接 DLL 化 `SceneCore` / `RenderCore` 作为第三方主入口。
4. Workbench package 先作为 prebuilt static package 稳定外部 consumer，再评估是否 DLL。
5. 业务扩展包以一个真实 vertical slice 验证，例如 Spray 或 Welding。

#### B7：形成阶段 C 的入口条件

阶段 B 完成后，必须明确阶段 C 先做哪个 facade。

推荐优先级：

1. `ProjectSimulationSDK`
   - 原因：第三方应用开发通常首先需要加载项目、查询项目对象、运行仿真任务。
2. `VisualizationSDK`
   - 原因：喷涂、焊接、路径规划都需要把路径、标记、热力图、颜色条、切面显示到 viewer。
3. `WorkbenchExtensionSDK`
   - 原因：GUI 扩展依赖项目和可视化贡献接口，适合稍后收敛。

### 5.10 验收标准

阶段 B 完成时应具备：

1. 一份完整 target/package 发布边界清单。
2. 每个 exported target 都有 public/internal 分类。
3. 每个 public target 都有 header 风险判断。
4. 每个 public target 都有 dependency 风险判断。
5. 每个 target 都有 DLL 候选级别。
6. 至少一版 SDK profile 草案。
7. 明确阶段 C 第一个要实现或收敛的 facade。
8. 明确哪些 target 暂时保持 static。
9. 明确哪些 headers 不应继续作为长期 SDK 暴露。

### 5.11 停止条件

阶段 B 是审计阶段。如果出现以下情况，应停止并报告，而不是直接进入修改：

1. 发现多个 package 的 namespace target 语义互相冲突。
2. 发现 public headers 大面积暴露内部类型，导致需要大规模 API 重构。
3. 发现 install/export 结果和源码 target 定义明显不一致。
4. 发现某个第三方依赖无法通过 SDK prefix 或 bootstrap 解析。
5. 需要一次性修改超过 50 个源/配置文件才能继续。
6. 需要重命名 public target 或移动一级目录。
7. 阶段 C 的第一个 facade 无法在现有模块边界下自然归属。

## 6. 阶段 B 之后的建议决策

阶段 B 结束后，建议优先进入 `ProjectSimulationSDK` facade 设计，而不是立即 DLL 化。

理由：

1. `RobotSDK` 已经覆盖机器人基础能力。
2. 第三方应用扩展更需要项目级能力：加载项目、枚举机器人/对象/工具、运行仿真任务、读取结果。
3. 项目级 facade 能减少 Workbench 和业务包对内部 `SimulationProject`、`SimulationRuntime` 的直接依赖。
4. `VisualizationSDK` 和 `WorkbenchExtensionSDK` 可以在项目级 facade 明确后更容易收敛。

DLL 化应在 facade API、headers、runtime 部署和 ABI/version 策略都明确后执行。

## 7. 当前明确不做的事情

1. 不把所有 `STATIC` target 批量改成 `SHARED`。
2. 不把 `SceneCore`、`RenderCore`、`RobotIO`、`Collision` 等内部组件直接定义为第三方主要开发入口。
3. 不在 API 未稳定前做 runtime plugin loader。
4. 不把 `RobotQtViewer` 改成业务语义所有者。
5. 不把硬件认证逻辑散落到每个未来 DLL；如未来需要 DLL load gate，应复用已有授权门槛，而不是重新设计一套认证路径。

