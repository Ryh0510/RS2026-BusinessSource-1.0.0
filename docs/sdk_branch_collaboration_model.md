# SDK 分支协作模型建议

## 背景问题

当前设想中存在三类状态：

- `branch1`：完整源码开发分支，包含 `SMRobotCore`、`SMRobotPlatform` 及其实现代码。
- `branch2`：面向团队其他成员交付的 SDK/开发包状态，`SMRobotCore` 和 `SMRobotPlatform` 以预编译 DLL、import lib、public headers、CMake package、文档和示例形式存在，源码被删除或隐藏。
- `branch3`、`branch4`：喷涂机器人、打磨机器人、移动搬运机器人等业务团队基于 `branch2` 继续开发的分支或项目。

真正的矛盾是：业务团队拿到的是预编译 SDK。如果他们发现现有 SDK 设计不够、接口缺失、行为需要调整，很多修改只能在 `branch1` 的源码中完成。此时如果 `branch3`、`branch4` 已经基于一个“删掉源码的 branch2”长期开发，需求回流、接口演进、版本升级和合并关系都会变得很困难。

## 核心结论

不建议把 `branch2` 当作一个长期开发分支，也不建议让业务团队在“删除 Core/Platform 源码后的同仓库分支”上长期演进并期望它能自然合并回 `branch1`。

更推荐的模型是：

```text
源码主线/发布分支
  -> 构建并发布 SDK artifact
      -> SDK consumer template 或业务项目仓库
          -> 业务团队开发喷涂/打磨/搬运等上层功能
              -> SDK 能力缺口通过 RFC/issue/API proposal 回流源码主线
                  -> 新版本 SDK 发布
                      -> 业务项目升级 SDK
```

也就是说，`branch1` 负责源码真实演进；`branch2` 最多是“自动生成的 SDK 分发快照”，不是日常功能开发分支；`branch3`、`branch4` 应该是 SDK 使用方项目，而不是 Core/Platform 的影子分支。

## 推荐仓库与分支形态

### 1. 源码仓库

源码仓库保留完整 `SMRobotCore`、`SMRobotPlatform`、`SMRobotApps` 等模块。

推荐分支：

- `main` 或 `develop`：日常集成主线。
- `release/sdk-x.y`：准备 SDK 发布、修复发布阻塞问题。
- `feature/core-xxx`：Core/Platform 源码功能开发。
- `feature/sdk-api-xxx`：SDK public API 增量设计与实现。

源码仓库中所有 SDK 变更必须经过：

- public header 审查；
- ABI/export 审查；
- package consumer 示例验证；
- release manifest 或版本记录更新。

### 2. SDK 发布物

SDK 应该优先以 artifact 形式交付，而不是靠一个手工维护的删源码分支交付。

推荐发布内容：

- `bin/`：DLL。
- `lib/`：import lib、CMake targets/config。
- `include/`：稳定 public headers。
- `docs/`：API、打包、兼容性、升级说明。
- `examples/`：最小 consumer、典型业务示例。
- `SMRobotSDKManifest.json`：组件、版本、构建配置、ABI 信息。
- export snapshot 或 ABI baseline。

如果确实需要一个 `branch2`，它应该是由 CI 或发布脚本生成的 distribution branch，只用于保存某个 SDK 版本的发布快照。它不应该接收人工功能开发，也不应该承担与 `branch1` 双向合并的责任。

### 3. 业务团队项目

喷涂、打磨、移动搬运等业务方向建议作为独立 consumer 项目或业务仓库存在。

业务项目通过：

```cmake
find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotSDK)
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS ProjectSimulationSDK RobotVisualizationSDK)
```

或类似方式消费 SDK。

业务项目可以提交：

- 上层应用逻辑；
- 业务机器人配置；
- 工艺参数；
- 自己的 UI/workbench；
- 插件或 adapter；
- 对 SDK 的 feature request；
- 能独立于 Core/Platform 实现的扩展代码。

业务项目不应该直接复制、修改、替换 SDK 内部源码。

## 需求回流流程

当业务团队发现 SDK 不够用时，不应直接在 `branch2` 上尝试修 Core/Platform，因为那里没有真实源码，也没有完整构建上下文。推荐按需求类型分流。

### A. 只需要上层扩展

例子：

- 新喷涂参数面板；
- 新任务流程；
- 新业务配置格式；
- 某个机器人品类自己的策略；
- 不影响 Core/Platform 通用语义的 adapter。

处理方式：

- 留在业务项目实现；
- 不改 SDK；
- 必要时通过已有 SDK 组合能力完成；
- 如果重复出现，再考虑抽象为通用 extension point。

### B. SDK 缺少必要查询或命令

例子：

- 业务需要查询某类 runtime 状态；
- 需要创建某种 project entity；
- 需要稳定访问碰撞、轨迹、工具坐标系、挂载关系；
- 需要把已有内部能力暴露给 consumer。

处理方式：

1. 业务团队提交 API proposal。
2. Core/Platform 维护者判断该能力属于哪个 SDK facade。
3. 在 `branch1` 或对应 feature 分支中实现。
4. 增加 public header、DTO、示例和 consumer smoke test。
5. 通过 ABI/export 审查后发布新 SDK。
6. 业务项目升级 SDK 版本。

关键原则：业务团队提出“需要什么能力”和“为什么需要”，源码团队决定“这个能力应该暴露在哪个层、以什么稳定接口暴露”。

### C. SDK 行为有 bug

例子：

- 某个模型加载失败；
- 碰撞结果错误；
- project 解析不稳定；
- runtime 状态不同步；
- DLL 缺少依赖。

处理方式：

1. 业务团队提交最小复现项目或数据。
2. 源码团队在 `branch1` 修复。
3. 增加 regression test 或 package consumer 测试。
4. 发布 patch 版本 SDK。
5. 业务项目升级 patch 版本。

这类问题应优先修在源码主线，避免业务团队用绕行代码固化错误行为。

### D. 需要破坏性接口变化

例子：

- 已有 public class/virtual interface 必须改签名；
- DTO 字段语义不兼容；
- 组件边界需要拆分；
- ABI 版本必须提升。

处理方式：

1. 先确认是否能通过新增 API 兼容解决。
2. 如果必须破坏，创建 SDK major/minor 演进计划。
3. 保留旧 API 一个 deprecation 窗口，除非尚未正式发布。
4. 同时维护迁移文档和示例。
5. 业务项目按版本窗口逐步升级。

破坏性变更不能由单个业务分支私下决定，必须作为 SDK release decision 管理。

## 接口设计原则

SDK 的 public API 不应该等同于把所有 Core/Platform 类导出。更稳妥的做法是按使用场景提供 facade 和 DTO。

推荐分层：

- `RobotSDK`：机器人模型、实例、关节、位姿、基础碰撞等 headless 能力。
- `ProjectSimulationSDK`：项目文档读取、验证、项目级运行状态或查询。
- `RobotVisualizationSDK`：机器人/项目状态到场景和渲染的可视化桥接。
- 业务扩展 SDK 或 plugin interface：喷涂、打磨、搬运等领域的可插拔能力。

Public API 应尽量满足：

- 不暴露内部 manager、singleton、SceneGraph 细节、OpenGL 资源、Qt 类型；
- 不让 consumer 依赖 private headers；
- 不把 STL/Eigen/第三方类型无控制地变成 ABI 承诺；
- 每个新增接口都有明确 owner、生命周期和测试示例；
- 能通过 package consumer 示例单独验证。

## 版本与发布策略

建议采用清晰的 SDK 版本规则：

- `MAJOR`：不兼容 API/ABI 变化。
- `MINOR`：向后兼容新增能力。
- `PATCH`：bug 修复，不改变 public API 语义。

每次发布记录：

- SDK 版本；
- commit/tag；
- 组件列表；
- Debug/Release 或平台配置；
- ABI/export snapshot；
- 已知兼容性限制；
- 升级说明。

业务项目不应该只记录“用了某个 branch2”，而应该记录“用了哪个 SDK version 或 artifact hash”。这样才能回溯问题。

## 临时需求的处理方式

业务团队有时会遇到发布节奏跟不上开发节奏的问题。可以使用几种临时机制，但要明确边界。

### Preview SDK

源码团队从 `branch1` 或 feature 分支发布 preview SDK，供业务团队提前验证。

要求：

- 明确标记为 preview；
- 不保证 ABI 长期稳定；
- 不能作为正式交付依赖；
- 反馈必须回流到正式 API 设计。

### Feature flag 或 experimental namespace

对于还在探索的能力，可以放入 experimental API。

要求：

- 命名和文档明确说明非稳定；
- 不和正式 API 混在一起；
- 进入正式版前必须重新审查。

### Inner-source 协作

如果业务团队成员有能力修改 Core/Platform，可以允许他们在源码仓库提交 PR，而不是在 SDK consumer 分支里改。

要求：

- 修改发生在源码仓库；
- 遵守模块边界和 ABI 规则；
- 由 Core/Platform owner review；
- 合并后通过 SDK 发布给所有业务团队。

这比把源码复制给某个业务分支更健康，因为改动会进入统一主线。

## 不推荐的做法

### 不推荐长期维护“删源码分支”

删除源码后的 `branch2` 可以作为发布快照，但不适合作为长期开发底座。原因：

- 很难从 `branch1` 合并新源码变化；
- 业务分支无法修复底层问题；
- 容易产生大量绕行代码；
- SDK 版本和源码 commit 关系不清晰；
- branch3/branch4 之间会各自解决同一个 SDK 缺口，后续难以统一。

### 不推荐把内部模块全部直接导出

把 `SMRobotCore`、`SMRobotPlatform` 每个模块都直接做成 DLL 给业务团队用，看起来灵活，但会快速扩大 ABI 承诺。

风险包括：

- private helper 被外部依赖；
- 内部重构成本变高；
- Qt/OpenGL/第三方依赖泄漏；
- Debug/Release、编译器版本、运行时库差异变成用户问题；
- 任意内部类都可能变成事实 public API。

更好的路径是：底层模块可以编译为 DLL，但正式交付面应由少数稳定 facade/component 控制。

## AI 辅助需求回流

可以让开发应用的团队使用 AI 生成需求回流材料，甚至生成面向 `branch1` 的初始补丁，但不建议让这些补丁绕过 Core/Platform owner 直接合入。

推荐把 AI 放在两个位置：

1. 在业务项目中，AI 帮使用方整理“我为什么现有 SDK 不够用”。
2. 在源码仓库中，AI 帮维护者根据该材料实现或改进 SDK。

业务团队可以让 AI 生成一个标准回流包：

```text
sdk-request/
  request.md                 # 需求说明、业务背景、期望行为
  current_workaround.md       # 现有绕行方案及其问题
  minimal_repro/              # 最小复现 consumer 工程或数据
  expected_api.md             # 期望 API 草案，不等于最终 API 承诺
  compatibility_notes.md      # 是否需要兼容旧 SDK，是否影响 ABI
  tests_or_acceptance.md      # 验收条件
```

如果业务团队没有 `branch1` 源码，可以让 AI 基于 public headers、SDK 文档、示例和错误日志生成 proposal，而不是生成不可验证的底层实现。这样的 proposal 质量通常已经足够让 Core/Platform 维护者判断归属层。

如果业务团队可以访问 `branch1`，可以让 AI 生成 draft PR，但要求：

- PR 必须提交到源码仓库，而不是 consumer 分支；
- 必须说明新增 API 属于哪个 facade/component；
- 必须包含最小 consumer 示例或测试；
- 必须通过 ABI/export/package gate；
- Core/Platform owner 可以重写 API 形状，不承诺接受业务侧 AI 生成的接口设计。

换句话说，AI 可以极大降低需求回流的表达成本和初始实现成本，但不能替代 SDK 治理。真正要合入 `branch1` 的仍然是经过 owner review、测试和发布流程验证的源码变更。

## 本地一键 SDK 发布

不购买 GitHub Actions、云 CI 或商业平台，也可以在当前框架下实现“一键构建 SDK”。云平台提供的是自动触发、隔离环境和 artifact 托管；本质流程仍然可以用本地脚本完成。

第一步不要直接做完整正式发布流程，而是先做一个本地 SDK 构建入口：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 -AllowDirtyTree
```

如果 install profile 因旧文件权限、半截安装产物或 DLL 占用失败，先清理 SDK install prefix 后重跑：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 -Configs Release -AllowDirtyTree -CleanInstall
```

如果前面已经构建成功，只想重跑安装和后续验证，可以跳过 configure/build：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 -Configs Release -AllowDirtyTree -SkipConfigure -SkipBuild -CleanInstall
```

这个脚本的目标是证明三件事：

- 当前源码能编译出 SDK 相关 targets；
- 当前源码能 install 成 SDK profile；
- install 后的 SDK 能被 package consumer 示例消费。

等这个闭环稳定之后，再做正式发布脚本。

推荐提供一个顶层脚本，例如：

```text
scripts/
  build_sdk.ps1
  release_sdk.ps1
```

其中 `build_sdk.ps1` 先顺序执行：

1. 检查工作树状态，确认是否允许从当前 commit 发布。
2. CMake configure。
3. 构建 SDK targets。
4. install SDK profile。
5. 生成 `SMRobotSDKManifest.json`。
6. 生成 ABI/export snapshot；本地快速构建默认不强制和 baseline 对比。
7. 运行 package consumer matrix。

如果需要正式 ABI gate，再显式传入 baseline：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 -Configs Release -AllowDirtyTree -AbiBaseline cmake\sdk_abi_baselines\SMRobotSDKExportSnapshot.v2.txt
```

正式 `release_sdk.ps1` 再增加：

1. 读取版本号，例如 `SMROBOT_SDK_VERSION` 或 release manifest。
2. 要求 clean tree 和固定 commit。
3. 生成 zip、sha256、release notes。
4. 可选：打 git tag，或把 artifact 复制到共享目录/Gitea release。

本地一键脚本可以先做到“手动运行一条命令”：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 -Configs Release -AllowDirtyTree
```

后续如果有 Gitea，也可以把同一套脚本接到 Gitea Actions、Jenkins、TeamCity、Buildbot、Windows 计划任务或内网构建机上。关键是把发布逻辑放在仓库脚本里，而不是只存在于某个平台配置里。

一键发布不等于无审查发布。建议分成两个命令：

- `scripts\build_sdk.ps1`：开发者本地快速构建和验证。
- `scripts\release_sdk.ps1`：正式产物生成，要求 clean tree、固定版本、ABI gate、manifest、release notes。

## AI 生成发布文档

大部分发布文档可以由 AI 生成初稿，但不应该全部无审查自动发布。

适合 AI 生成的内容：

- release notes 初稿；
- SDK upgrade guide 初稿；
- API proposal 摘要；
- changelog 分类；
- consumer 示例说明；
- 已知限制清单；
- 从 manifest/export snapshot 提取的组件列表；
- 根据测试输出整理的验证报告。

不适合完全交给 AI 自动决定的内容：

- 是否提升 `MAJOR/MINOR/PATCH`；
- 某个 API 是否正式稳定；
- 是否接受 ABI break；
- 是否保留兼容旧 API；
- 法务、授权、商业交付说明；
- 对客户承诺的性能、稳定性和安全性结论。

推荐做法是让脚本收集事实，让 AI 生成文档初稿，让 maintainer 审查后发布。

可以把发布资料分成两类：

- 机器事实：manifest、commit、tag、artifact hash、export snapshot、测试日志。
- 人类解释：release notes、迁移说明、兼容性解释、推荐升级路径。

AI 最适合把机器事实和 git diff 转换成人类解释，但最终版本号和兼容性承诺应由负责人确认。

## 面向第三方 AI 的 SDK 上下文包

如果给其他团队一个可二次开发的库，建议同时提供“给人读的文档”和“给 AI 读的上下文包”。未来业务团队很可能会让自己的 AI 助手基于 SDK 写代码，如果上下文不完整，AI 会猜内部类、猜 CMake target、猜 DLL 部署方式，反而制造错误。

建议在 SDK 包中增加：

```text
docs/
  AI_CONTEXT.md
  SDK_API.md
  SDK_PACKAGING.md
  SDK_EXTENSION_BOUNDARIES.md
  UPGRADE_GUIDE.md
  TROUBLESHOOTING.md
examples/
  PackageConsumer/
  RobotQuickStart/
  ProjectQuickStart/
templates/
  api_request_template.md
  bug_report_template.md
```

`AI_CONTEXT.md` 应该明确告诉第三方 AI：

- 只能 include 哪些 public headers；
- 只能 link 哪些 imported targets；
- 哪些模块是内部实现，不允许依赖；
- DLL 需要如何部署；
- 常见任务应该调用哪个 facade；
- 常见错误如何排查；
- 如果 SDK 不够用，应该生成什么 request package；
- 不要建议用户修改 SDK 内部源码或复制 private headers。

一个好的 AI 上下文包能把“第三方 AI 的自由发挥”限制在正确边界内。它不是为了约束人，而是为了避免 AI 误把内部实现当成可依赖接口。

## 推荐决策表

| 场景 | 应该在哪里改 | 是否发布新 SDK | 备注 |
| --- | --- | --- | --- |
| 业务 UI 或流程变化 | 业务项目 | 否 | 除非需要通用 extension point |
| 新机器人品类配置 | 业务项目或 Project SDK | 视情况 | 通用项目语义应回流 |
| SDK 缺少查询接口 | `branch1` 源码 | 是 | 优先新增兼容 API |
| SDK 内部行为 bug | `branch1` 源码 | 是，通常 patch | 必须有复现或测试 |
| 破坏性接口变化 | SDK release 分支 | 是，major/minor | 需要迁移文档 |
| 临时探索能力 | preview SDK 或 experimental API | 可选 | 不作为长期正式依赖 |
| 业务团队想改 Core/Platform | 源码仓库 PR | 合并后发布 | 不在 consumer 分支私改 |

## 建议落地步骤

1. 明确 SDK 正式交付面：先确定哪些 facade/component 是稳定 API，哪些只是内部 DLL。
2. 建立 SDK release artifact：由 CI 或脚本从源码分支生成 zip、manifest、CMake package、examples。
3. 将 `branch2` 定义为 distribution snapshot，禁止作为人工开发分支。
4. 为喷涂、打磨、搬运建立独立 consumer 项目或模板仓库。
5. 建立 API proposal 模板：业务需求、当前绕行方式、期望接口、最小示例、兼容性影响。
6. 建立 preview SDK 通道：用于业务团队提前验证未正式稳定的能力。
7. 建立 SDK upgrade 规则：业务项目记录 SDK version/hash，并定期升级。
8. 建立 owner review：Core/Platform public API 由对应模块 owner 审查后才能发布。
9. 建立本地一键发布脚本：先在本机可靠运行，再接入 Gitea 或其他 CI。
10. 建立 `AI_CONTEXT.md` 和 request/bug 模板，降低第三方 AI 误用 SDK 的概率。

## 对当前项目的建议

结合现有文档，当前项目已经有 `SDK_PACKAGING.md`、`SDK_EXTENSION_BOUNDARIES.md`、`sdk_abi_export_policy.md` 等基础。下一步不应该简单扩大 DLL 导出面，而应该继续把交付面收敛成几个稳定 SDK facade：

- Core 层继续以 `RobotSDK` 作为主入口。
- Platform 层按能力拆成 `ProjectSimulationSDK`、未来可视化 SDK 或更窄的业务 facade。
- `SceneCore`、`RenderCore`、`RobotRenderBridge` 这类模块即使编译为 DLL，也应谨慎决定哪些 API 是正式 ABI。
- 业务团队优先通过 consumer 项目开发，上层能力成熟后再沉淀到 SDK。

最重要的是把“业务需要新能力”变成正常发布流程，而不是变成分支拓扑问题。分支只保存代码状态，不能替代 API 治理、版本治理和 owner review。

## 推荐的工作流示意

```text
业务团队发现 SDK 能力缺口
        |
        v
提交 issue / RFC / 最小复现
        |
        v
Core/Platform owner 判断归属层
        |
        +--> 业务层能力：留在业务项目或插件
        |
        +--> 通用 SDK 能力：进入源码分支实现
                          |
                          v
                 public API + tests + examples
                          |
                          v
                 ABI/package/release gate
                          |
                          v
                    发布 SDK x.y.z
                          |
                          v
                    业务项目升级 SDK
```

## 一句话原则

`branch1` 是源码真相，SDK artifact 是交付真相，业务项目是使用真相；不要让一个删源码的 `branch2` 同时承担交付、开发、集成和需求回流四种职责。
