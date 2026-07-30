# 第一阶段 P0 Runtime 与发布策略补充修订计划

状态：已完成
更新时间：2026-07-20

## 1. 修订目标

在不改变项目文件 `data/...` 可移植格式、不强制携带完整模型库的前提下，完成以下发布契约：

1. Runtime 优先从可执行程序旁的 `data` 目录或显式 `SMROBOT_DATA_ROOT` 加载资产，源码目录只作为开发环境后备。
2. Runtime 默认不复制 `data`；仅在指定 `-IncludeRuntimeData` 时携带模型数据。
3. 统一发布默认复用已有 Core/Platform PreBuild，不重新编译、不单独生成 SDK ZIP；仅在指定 `-IncludeSdk` 时执行双配置 SDK 构建、矩阵验证和归档。
4. 首次发布版本基线调整为 `1.0.0`，发布者可通过 `-Version` 指定具体版本。

## 2. 不变量与所有权

- 项目资产路径格式仍为 `data/...`，不写入源码绝对路径，也不改成 `../../data/...`。
- 路径解析规则由 `SMRobotPlatform/SimulationProject/AssetResolver` 统一拥有；Viewer、Runtime 和 GUI 不分别补路径。
- `SMROBOT_DATA_ROOT` 的正式含义是“直接包含 `Spray420`、`SprayDEC` 等内容的 data 目录”。
- 为兼容现有用法，解析器仍接受将 `SMROBOT_DATA_ROOT` 指向“包含 data 子目录的上级目录”，但新文档不再推荐该方式。
- Business Source 仍内置一份可消费的 Core/Platform PreBuild；默认发布只是复用指定 prefix，避免每次重编。
- 不新增第三方依赖，不修改项目 JSON schema，不修改公开 C++ API。

## 3. 修改任务

### P0-R1：本地 data 优先解析

文件：

- `SMRobotPlatform/SimulationProject/src/AssetResolver.cpp`
- `SMRobotPlatform/SimulationProject/regression/ProjectV3IoSmokeTest/main.cpp`

动作：

- 对 `data/...` 去掉逻辑前缀后与 `dataRootPath` 拼接。
- 在源码根回退之前检查 Runtime/显式 data root。
- `${DATA_DIR}` 搜索根采用相同规范化规则。
- 保留旧的“上级目录 + data/...”兼容查找。
- 增加同名 Runtime 资产与源码后备资产并存时，Runtime 资产必须胜出的回归断言。

### P0-R2：发布产物选择

文件：

- `scripts/release_phase1.ps1`
- `scripts/package_app_release.ps1`

动作：

- 新增 `-IncludeSdk`，默认关闭。
- 新增 `-SdkPrefix`，默认复用 `build/phase1_sdk_install`。
- 缺少可复用 prefix 时提前报错，并提示使用 `-IncludeSdk`。
- 顶层 manifest 只收录本次明确生成的两个或三个 ZIP，避免旧 SDK ZIP 混入。
- Runtime 继续默认 external data；README 与 manifest 明确 data root 契约。

### P0-R3：版本控制

文件：

- `CMakeLists.txt`
- `scripts/release_phase1.ps1`
- `scripts/package_app_release.ps1`
- `README.md`

动作：

- 项目与统一发布默认版本改为 `1.0.0`。
- 保留 `-Version` 覆盖能力，允许发布者自行决定正式版或预览版标签。

## 4. 验证

1. PowerShell AST 解析发布脚本。
2. CMake configure，确认项目版本为 `1.0.0`。
3. 构建并运行 `SimProject-V3IoSmokeTest`，确认 `data/...` 优先命中 Runtime data root。
4. 构建 `RobotQtViewer` Release。
5. 从导出 Runtime 目录加载真实项目，检查日志中的资产路径不再回退源码目录（携带 data 时）。
6. 使用默认统一发布参数，确认只收录 Business Source 与 Runtime ZIP，并复用已有 SDK prefix。
7. 使用 `-IncludeSdk` 的参数/语法路径检查，确认 SDK 为显式可选产物。
8. 执行 `git diff --check` 和修改文件行尾检查。

## 5. 风险与停止条件

- 如果修改要求批量重写项目 JSON，则停止并回到中央解析器方案。
- 如果 Business Source 无法复用已安装 SDK prefix，则不静默重建，必须明确报错。
- 如果现有代码依赖 `SMROBOT_DATA_ROOT` 指向上级目录，则保留兼容候选，不破坏旧用法。
- 不清理或覆盖当前工作树中用户已有的 `cmake/ProjectPackagesConfigSetting.cmake` 修改。

## 6. 实际执行结果

- `AssetResolver` 已将 `data/...` 规范化为 data root 内部相对路径，并在源码后备之前检查 Runtime/显式 data root。
- 保留了旧的“`SMROBOT_DATA_ROOT` 指向包含 data 子目录的上级目录”兼容路径。
- `ProjectV3IoSmokeTest` 新增 Runtime data 与源码后备同名资产竞争用例，Release 测试通过。
- `RobotQtViewer` 与 `RenderCoreShaderResources` Release 构建通过；真实项目 profile 日志确认 `420-tool.STL` 从可执行程序旁的 `data/Spray420` 加载。
- Core/Platform SDK prefix 已完成 Debug/Release 增量构建、重新安装、ABI 检查和消费矩阵验证。
- 默认统一发布使用 `1.0.0-local-check` 完整执行通过，耗时约 7 分 49 秒；只生成 Business Source 与 Runtime 两个 ZIP，没有生成 SDK ZIP。
- Runtime manifest 为 `dataMode=external`、`dataRootContract=data-directory`。
- Runtime 二进制复制阶段会显式排除构建目录残留的 `bin/data`；默认包复验为 `dataEntryCount=0`，避免 manifest 声明 external 但 ZIP 意外携带测试数据。
- 顶层 release manifest 明确记录 `sdkArtifactIncluded=false`，且只统计本次明确生成的两个 artifact。
- VS2019 在过长的独立验证构建目录中出现过 tracking-log 路径错误；切换到现有短路径 `build/phase1_app_release` 后主程序构建通过，不属于本次代码缺陷。
