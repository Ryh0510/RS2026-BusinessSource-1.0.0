# RS2026 第一阶段交付进展与修订报告

更新时间：2026-07-20

## 1. 报告目的

第一阶段需要形成三类互相独立但可由同一发布入口生成的交付物：

1. `App Runtime Bundle`：解压后可以直接启动 `RobotQtViewer` 的 Windows Release 程序包。
2. `SMRobotCorePlatformPreBuild`：同时包含 Debug/Release 的 `SMRobotCore`、`SMRobotPlatform`、必要 `Common` 组件、头文件、import library、DLL 和 CMake package。
3. `Business Source Package`：删除 `SMRobotCore`、`SMRobotPlatform` 源码，保留喷涂、打磨、Workbench、应用等上层源码，并通过 PreBuild 继续构建。

当前阶段所称的 “SDK” 更准确地说是内部全组件 PreBuild。面向外部客户、只承诺稳定 facade 的正式 SDK 属于后续发布边界收敛工作，不作为 P0 的前置条件。

## 2. 当前基线

- 分支：`branch-codex-dev`
- 基线提交：`69ee549a1c2cc9cb721f0a366377593647d6d00b`
- 工作树：审计开始时干净，并与 `origin/branch-codex-dev` 对齐。
- 当前目录已有 CMake configure/generate 结果，但清理后没有可作为当前交付证据的 SDK、业务源码包或完整应用发布包。
- 历史提交证明 SDK consumer matrix 曾通过 Debug/Release，业务源码 staging 曾在缺少 Core/Platform 源码时构建 `SprayCore Release`；这些结果需要在当前目录和当前提交上重新验证。

## 3. 当前完成情况

### 3.1 App Runtime Bundle

已有能力：

- `RobotQtViewer` 使用 `$<TARGET_RUNTIME_DLLS>` 复制可解析的动态库。
- Windows 构建会调用 `windeployqt` 部署 Qt DLL 和插件。
- `RenderCoreShaderResources` 已将默认 shader 编译进独立 DLL，发布包不必依赖 `data/shader_gen3`。
- Core、Platform 和主要渲染组件已经以 DLL 形式输出到配置对应的 `bin` 目录。

未闭环问题：

- 没有专门生成 Runtime staging、ZIP、SHA256 和 manifest 的一键脚本。
- 没有统一复制 `config`、选定 license、运行说明和第三方声明。
- `windeployqt` 使用 `--no-compiler-runtime`，发布包需要明确 VC++ Runtime 前置条件。
- 默认项目和多个运行时资产上下文仍优先使用编译期 `PROJECT_SOURCE_PATH`，程序换目录或离开源码树后可能失败。
- 没有在临时目录中启动发布包的自动烟测。

结论：构建输出目录具备较多运行依赖，但还不是正式、可搬迁的程序发布物。

### 3.2 Core/Platform PreBuild

已有能力：

- `scripts/build_sdk.ps1` 已包含 configure、目标构建、profile install、manifest、ABI/export snapshot 和 consumer matrix。
- `SdkPrefix` 可由调用方指定。
- 已有 `core_platform_thin_internal` 和 `core_platform_standalone` profile。
- 已有 `cmake/CreateSmRobotSdkReleaseArtifact.cmake` 生成 ZIP、SHA256 和发布说明。
- `UsingPrebuilt_SMRobotCore`、`UsingPrebuilt_SMRobotPlatform` 会关闭对应源码 component，并从显式 SDK prefix 查找 CMake package。

未闭环问题：

- `build_sdk.ps1` 默认只构建 Release，不符合第一阶段默认同时交付 Debug/Release 的要求。
- 正式 archive 仍是独立手工步骤，没有进入统一发布入口。
- `SMRobotPlatform` 声明可导出 `RobotPlatformAuthorization`，但推荐 profile 没有安装该 component，component 白名单和安装闭包不一致。
- 当前 manifest 以构建路径信息为主，正式追溯信息仍需继续完善。
- `standalone` profile 只表示 Core/Platform SDK 自身尽量自包含，不等于业务应用源码可以不准备 Qt、data 等完整开发依赖。

结论：PreBuild 主体机制已经存在，是当前完成度最高的交付链；P0 主要负责默认双配置、组件一致性、正式归档和当前基线复验。

### 3.3 源码/PreBuild 混合构建

已有能力：

- 显式 PreBuild 根目录：`SMROBOT_PREBUILT_PACKAGE_ROOT`。
- 显式开关：`UsingPrebuilt_SMRobotCore`、`UsingPrebuilt_SMRobotPlatform`。
- 开关启用时自动关闭 `BuildPackage_SMRobotCore`、`BuildPackage_SMRobotPlatform`。
- Core/Platform 源码目录存在时只读取 package 配置，不添加源码 component；源码目录缺席时可以直接跳过目录。

当前证据边界：

- 历史业务 staging 已证明 `SprayCore Release` 可以构建。
- 尚无当前提交下、Core/Platform 源码完全缺席时，`RobotQtViewer Debug/Release` 都构建成功的证据。

结论：CMake 机制成立，但完整 GUI 应用的无源码双配置验证仍是 P0 的核心门槛。

### 3.4 Business Source Package

已有能力：

- `scripts/package_business_source.ps1` 可以复制源码到 staging。
- 会从 staging 删除 `SMRobotCore` 和 `SMRobotPlatform`。
- 会复制指定 SDK prefix 到 `PrebuiltPackages`。
- 会生成 `sdk.lock.json` 和 `configure_business_prebuilt.ps1`。
- 支持 configure/build 验证、ZIP 和 SHA256。

主要问题：

- 当前采用“复制整个仓库再按黑名单排除”的方式，默认排除目录没有 `license`，会把仓库中的 `.LIC` 和 `AuthN` 文件带入交付包。
- 缺少发布后敏感文件扫描和 Core/Platform 源码缺席断言。
- 当前历史验证目标是 `SprayCore Release`，不足以证明完整业务应用开发包成立。
- `thirdparty` 和 `data` 默认作为外部依赖，需要在总体交付 manifest 和下游配置中明确其版本和实际位置。

结论：第一版自动裁剪已经可用，但授权文件安全和完整应用验证必须在 P0 修复。

## 4. P0/P1/P2 修订路线

### P0：第一阶段交付闭环

目标：从干净提交一键生成三类交付物，并在没有 Core/Platform 源码、没有原源码路径依赖的环境中完成自动验证。

必须完成：

1. 统一可执行程序根、配置根和数据根解析，源码路径只作为开发 fallback。
2. 新增 App Runtime Bundle staging、manifest、ZIP、SHA256 和启动烟测入口。
3. PreBuild 默认同时生成 Debug/Release。
4. 修复 SDK profile/component 不一致。
5. 业务源码包默认排除全部授权材料，只允许显式复制指定 license。
6. 在 staging 删除 Core/Platform 源码后构建 `RobotQtViewer Debug/Release`。
7. 增加阶段一总发布入口，统一输出和验证三类 artifact。

### P1：发布治理与可维护性

目标：让第一阶段产物具有稳定版本、依赖锁定、审计信息和团队使用规范。

建议内容：

1. 为 Runtime、PreBuild、Business Source 定义正式 manifest schema。
2. 记录主仓库、thirdparty、data commit 和文件 hash。
3. 将业务源码包从黑名单复制逐步改成顶层允许列表。
4. 形成 `THIRD_PARTY_NOTICES`、升级说明、故障排查和 AI 使用上下文。
5. 建立 preview/release 两种发布门槛。
6. 更新与源码不一致的 SDK 边界和 target 分类文档。
7. 将本地脚本接入 Gitea/Jenkins/内网构建机时仍复用同一发布逻辑。

### P2：正式外部 SDK 边界

目标：把内部全组件 PreBuild 收敛为少数稳定 facade，降低 ABI 承诺和第三方依赖泄漏。

建议内容：

1. 区分 `InternalFullPreBuild` 和 `ExternalFacadeSDK` profile。
2. 以 `RobotSDK`、`ProjectSimulationSDK`、`VisualizationSDK` 为主要公开入口。
3. 审计并减少 public headers 对内部类型、Qt、OpenGL、FCL、STL/Eigen ABI 的暴露。
4. 收敛仍使用自动符号导出的渲染链组件。
5. 建立 SDK 版本、ABI 兼容和废弃窗口。
6. 在 extension API 稳定后再讨论运行时插件化。

## 5. 第一阶段最终产物建议

```text
RS2026-<version>/
  RS2026-Runtime-win64.zip
  SMRobotCorePlatformPreBuild-vs2019-x64.zip
  RS2026-BusinessSource.zip
  release-manifest.json
  SHA256SUMS.txt
  verification-report.txt
```

`thirdparty`、模型/data 可以继续作为独立 artifact 管理，不要求每次 Core/Platform 小改都重新复制；但总 manifest 必须明确它们的版本、commit 和下游路径要求。

## 6. 第一阶段完成标准

- 一条命令生成三类交付物。
- Runtime ZIP 解压到任意临时目录后可以启动，不访问原源码目录。
- Qt 插件、项目 DLL、第三方 DLL、shader resource DLL 和选定 license 部署正确。
- PreBuild manifest 同时声明 Debug/Release，两个配置的 consumer matrix 均通过。
- `SMRobotCore`、`SMRobotPlatform` 源码目录完全不存在时，`RobotQtViewer` Debug/Release 均构建成功。
- Business Source ZIP 不包含 Core/Platform 源码、其他机器 license/AuthN、私钥或内部签发材料。
- 所有 ZIP 都有版本、commit、manifest 和 SHA256。
- 正式发布从干净提交生成，不依赖 `-AllowDirtyTree`。

## 7. P0 执行更新

P0 已于 2026-07-20 完成。当前代码已经具备三类交付物的独立入口和统一入口：

- `scripts/package_app_release.ps1`：生成可搬迁的 Windows Runtime Bundle。
- `scripts/build_sdk.ps1`：默认生成 Debug/Release `core_platform_standalone` PreBuild，并执行 ABI 与消费矩阵门禁。
- `scripts/package_business_source.ps1`：生成无 Core/Platform 源码的业务开发包，并支持 Debug/Release 主程序验证。
- `scripts/release_phase1.ps1`：顺序生成上述三类产物及顶层发布清单。

本次一键验证生成的正式结构与第 5 节建议一致。P1 应继续解决 manifest 去本机路径化、第三方许可清单、CI 接入和正式 preview/release 治理；P2 仍负责把内部全组件 PreBuild 收敛为稳定的外部 facade SDK。
