# 第一阶段 P0 交付闭环详细修订计划

状态：已完成

更新时间：2026-07-20

## 1. 目标

在不改变机器人、项目、碰撞和 Workbench 业务语义的前提下，完成第一阶段最小正式交付闭环：

1. 可搬迁启动的 `RobotQtViewer` Release Runtime Bundle。
2. 默认包含 Debug/Release 的 Core/Platform Internal PreBuild。
3. 删除 Core/Platform 源码后仍可构建完整 `RobotQtViewer` 的 Business Source Package。
4. 一个统一入口在指定输出目录生成上述三类 artifact、manifest、SHA256 和验证报告。

## 2. 非目标

- 不引入新的第三方依赖或包管理器。
- 不建立运行时插件系统。
- 不在 P0 内完成 External Facade SDK 的最终 public API 收敛。
- 不重构机器人、渲染、碰撞、项目文档或 Workbench 业务逻辑。
- 不把完整 785MB `data` 模型库强制塞入 Runtime Bundle；数据可以通过显式参数或独立 artifact 提供。
- 不复制整个授权目录，不处理 license 签发流程。

## 3. 设计不变量和所有权

### 3.1 运行时路径

不变量：发布后的程序不能把编译机源码根作为正常运行的首选路径。

所有者：`SMRobotPlatform::SimulationProject`。

策略：

1. 运行时优先读取显式环境变量。
2. 其次以可执行程序目录作为 app root。
3. `config`、`data` 从 app root 或显式 root 解析。
4. `PROJECT_SOURCE_PATH`、`DATA_PATH` 只作为源码开发 fallback。
5. `RobotQtViewer`、`RobotViewerCore`、`SimulationRuntime` 和 `ProjectSession` 复用同一解析 API，不各自拼路径。

### 3.2 Runtime Bundle

不变量：staging 中包含启动所需 exe、DLL、Qt plugins、shader resource DLL、配置和显式选择的 license；发布脚本不得复制整个授权目录。

所有者：`scripts/package_app_release.ps1`。

### 3.3 PreBuild

不变量：推荐内部 profile 默认安装 Debug/Release；package 声明的 component 和 profile 实际安装的 component 一致。

所有者：`scripts/build_sdk.ps1`、`cmake/InstallSmRobotSdkProfile.cmake`。

### 3.4 Business Source

不变量：交付包不包含 Core/Platform 源码，不包含未显式选择的 license、AuthN 或密钥材料；完整应用通过 PreBuild 构建。

所有者：`scripts/package_business_source.ps1`。

## 4. 修改任务

### 任务 P0-1：固化分析和计划

文件：

- `docs/phase1_delivery_progress_and_revision_report.md`
- `plans/phase1_p0_delivery_closure_plan.md`
- `plans/README.md`

动作：保存现状、P0/P1/P2 路线和第一阶段完成标准，并将本计划加入有效计划索引。

验收：文档路径有效，引用不指向已删除文件。

### 任务 P0-2：集中运行时根目录解析

预期文件：

- `SMRobotPlatform/SimulationProject/include/SimulationProject/RuntimePaths.h`
- `SMRobotPlatform/SimulationProject/src/RuntimePaths.cpp`
- `SMRobotPlatform/SimulationProject/src/ProjectSession.cpp`
- `SMRobotPlatform/SimulationRuntime/src/ProjectSimulationRuntime.cpp`
- `SMRobotApps/RobotViewerCore/ProjectRuntimeBuilder.cpp`
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`
- `SMRobotApps/RobotQtViewer/MainWindow.cpp`
- `SMRobotApps/RobotQtViewer/RobotQtViewerCollisionWorkbenchServicesAdapter.cpp`
- `SMRobotApps/RobotQtModules/Shared/ViewportReloadWorkflowController.cpp`

动作：

- 增加跨 Qt/headless 可用的 `RuntimePaths`。
- 支持 `SMROBOT_APP_ROOT`、`SMROBOT_CONFIG_ROOT`、`SMROBOT_DATA_ROOT` 环境变量。
- Windows 通过当前进程 executable path 获得 app root。
- 将生产路径中的直接 `PROJECT_SOURCE_PATH` 替换为集中解析。
- 保留编译期路径作为开发态 fallback。

验收：

- 生产链路不再直接以 `PROJECT_SOURCE_PATH` 构造默认项目、项目 base path 或 data root。
- 源码树构建仍能使用原 `config`、`data`。
- 外部 app root/data root 能覆盖开发 fallback。

### 任务 P0-3：应用发布包入口

预期文件：

- `scripts/package_app_release.ps1`
- `SMRobotApps/RobotQtViewer/main.cpp`

动作：

- 一条命令完成 configure、Release build、staging、ZIP 和 SHA256。
- staging 复制配置对应 `bin` 内容，因此保留 `$<TARGET_RUNTIME_DLLS>`、`windeployqt` 和 shader resource DLL 结果。
- 复制 `config`。
- 仅通过 `-LicenseFile` 复制指定 `.LIC`；默认不复制授权目录。
- data 默认不内嵌，可通过 `-IncludeData` 显式复制。
- 生成 runtime manifest、README 和发布 hash。
- 增加无需加载指定模型即可自动退出的启动 smoke 参数。

验收：

- staging 中存在 exe、Qt `platforms/qwindows.dll`、shader resource DLL、config 和 manifest。
- ZIP 和 `.sha256` 存在。
- 从 staging 工作目录启动并按 smoke 参数正常退出。

### 任务 P0-4：双配置 PreBuild 与组件一致性

预期文件：

- `scripts/build_sdk.ps1`
- `cmake/InstallSmRobotSdkProfile.cmake`

动作：

- 默认配置调整为 `Debug;Release`。
- 默认目标和推荐 profile 补齐 `RobotPlatformAuthorization`。
- 增加可选正式 archive 参数，复用现有 release artifact CMake 脚本。
- 正式 archive 使用 ABI baseline gate。

验收：

- manifest 的 `configs` 同时包含 Debug/Release。
- Debug/Release `.lib`、DLL、Targets 配置均存在。
- consumer matrix 两个配置通过。
- SDK ZIP 和 SHA256 可由同一脚本生成。

### 任务 P0-5：业务源码包安全门槛

预期文件：

- `scripts/package_business_source.ps1`

动作：

- 默认排除 `license`。
- 增加可选 `-LicenseFile`，只复制指定 `.LIC`。
- 归档前断言 `SMRobotCore`、`SMRobotPlatform`、`.git` 不存在。
- 扫描 `.AuthN`、`.pfx`、`.pem`、`.key`、`.keymgmt` 等敏感后缀；除显式 license 外发现即失败。
- 验证支持 Debug/Release 多配置。

验收：

- 默认业务包不含任何 `.LIC`、`.AuthN` 或密钥材料。
- staging 中无 Core/Platform 源码。
- `RobotQtViewer` Debug/Release 使用复制后的 PreBuild 构建成功。

### 任务 P0-6：第一阶段统一发布入口

预期文件：

- `scripts/release_phase1.ps1`

动作：

- 接受版本、输出根、用户配置、license、thirdparty/data 路径。
- 顺序调用 SDK、业务源码和 Runtime 三条发布链。
- 正式模式要求 clean tree。
- 汇总 artifact hash 和验证结果到顶层 manifest/report。

验收：指定输出根下存在三种 ZIP、SHA256 和顶层发布 manifest。

### 任务 P0-7：验证和审计

文件：`docs/agent_change_audit.md` 和必要的现有 regression/package example 文件。

验证顺序：

1. PowerShell AST 语法检查。
2. CMake configure。
3. `RobotQtViewer`、`RenderCoreShaderResources` Release 构建。
4. Runtime Bundle staging 启动烟测。
5. PreBuild Debug/Release build、install、ABI gate、consumer matrix。
6. Business Source staging 删除源码后的 `RobotQtViewer` Debug/Release 构建。
7. 敏感文件扫描。
8. JSON 解析、Markdown 引用、`git diff --check` 和 CRLF 检查。

## 5. 执行波次

### Wave 1：路径与计划基础

- P0-1
- P0-2

### Wave 2：三个发布入口

- P0-3
- P0-4
- P0-5

### Wave 3：统一发布与闭环验证

- P0-6
- P0-7

Wave 2 依赖 Wave 1 的路径可迁移性；Wave 3 依赖三个独立入口分别可用。

## 6. 风险与停止条件

- 如果完整 data 闭包必须自动计算，超出 P0；先使用独立 data artifact 或 `-IncludeData`。
- 如果 `RobotQtViewer` 无源码构建暴露 public/private header 大面积泄漏，只修阻断当前应用的最小边界，不在 P0 批量重构 API。
- 如果两个连续构建失败来自不同架构问题，停止扩大修改，先形成失败分类。
- 不为通过烟测关闭正式发布授权；正式默认保持 `RS2026_LICENSE_ENFORCEMENT_MODE=Enforce`。
- 不自动选择或复制客户 license。

## 7. P0 完成定义

- 本计划全部任务完成。
- 三类 artifact 可独立生成。
- 当前提交上所有自动验收通过，或仅剩明确要求人工确认的 GUI 视觉项。
- 无未说明的 public API、项目 schema 或模块依赖变化。
- `docs/agent_change_audit.md` 记录实际验证证据和剩余限制。

## 8. 实际执行结果

P0 已于 2026-07-20 完成，计划内任务 P0-1 至 P0-7 全部落地。

- 新增统一运行时路径解析，生产链优先使用显式环境根和可执行程序目录，编译期源码路径只保留为开发 fallback。
- 新增应用 Runtime Bundle 脚本，已验证 Qt platform plugin、项目 DLL、shader resource DLL、指定 license、manifest、ZIP、SHA256 和隐藏启动冒烟。
- PreBuild 默认改为 `core_platform_standalone`，同时生成 Debug/Release；独立消费矩阵 Debug 21/21、Release 21/21 通过。
- 保留历史 ABI `v1`，建立第一阶段正式 ABI `v2` 基线；强制基线比较和正式 SDK 归档通过。
- 业务源码包删除 `SMRobotCore`、`SMRobotPlatform` 后，`RobotQtViewer` Debug/Release 均在独立短路径构建树中成功构建。
- 业务包仅包含显式选择的一个 `.LIC`，未发现额外 `.AuthN`、证书或私钥材料。
- `scripts/release_phase1.ps1` 从独立构建目录一键生成三类 ZIP、顶层 manifest、`SHA256SUMS.txt` 和验证报告，全流程成功，首次全量耗时约 26 分 44 秒。

验证产物位于 `build/phase1_release_p0`，属于本地构建结果，不提交到源码仓库。
