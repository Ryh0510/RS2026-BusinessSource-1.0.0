# Business Source 与预编译 SDK 依赖边界修订计划

## 1. 状态与目标

- 当前状态：已完成并通过本地验证。
- 默认验证环境：`USER_TANGTANG_4090_Windows`、Visual Studio 2019、x64、Debug/Release。
- 目标：让导出的 `Common`、`SMRobotCore`、`SMRobotPlatform` 只暴露真实公共依赖；让 Business Source 默认消费这些预编译包；删除当前正式产品已经不使用的 PCL、OpenNI2、LZ4、FLANN 全局依赖。
- 非目标：不把 Qt 或 OMPL 纳入 Core/Platform SDK；本轮不完成 FCL/OctoMap/libccd 的内部封装；不改变 PCD 文件格式和现有点云业务语义。

## 2. 所有权与不变量

### 2.1 CustomLog 与 Boost

- `Common::CustomLog` 的公开头文件不得包含 Boost 头文件，也不得在公开 ABI 中传递 Boost 类型、异常、回调或全局状态。
- Boost 是 `CustomLog` 的私有构建依赖和运行时实现依赖，不是 SDK 消费者的开发依赖。
- `Common::CustomLog` 导出时只携带最终 DLL 真实需要的 Boost Debug/Release 运行 DLL，不导出 Boost 头文件、CMake package 和 import libraries。
- 外部消费者应能在没有 Boost CMake package 的环境中完成 `find_package(Common COMPONENTS CustomLog)`、链接和运行。

### 2.2 Business Source

- Business Source 默认使用 Prebuilt `Common`、`SMRobotCore`、`SMRobotPlatform`，不重新编译 `CustomLog`、Core 或 Platform。
- Business Source 应携带 Core/Platform SDK 所需的公共第三方开发依赖，例如当前尚未封装的 FCL/OctoMap/libccd。
- Qt 和 OMPL 继续由 Business Source 使用者外部提供，不进入 Core/Platform SDK。

### 2.3 点云与 PCL

- RobotQtViewer 的 PCD 导入由 `SensorCore::PcdPointCloudLoader` 实现，支持 ASCII、Binary 和 Binary Compressed/LZF；不得因移除 PCL 而改变该路径。
- 当前正式源码没有 PCL API、头文件或 target 依赖，因此 PCL 和 OpenNI2 不再作为全局配置依赖。

### 2.4 LZ4 与 FLANN

- 当前仓库没有 `RadiationTherapySystem` 源码，也没有正式目标引用 LZ4 或 FLANN。
- 删除用户配置中的全局查找；未来若恢复 RadiationTherapy，由对应 package/target 自行声明依赖。

## 3. 当前问题

1. `Common/CustomLog/TargetConfigSetting.cmake` 把 `Boost::log` 和 `Boost::log_setup` 声明为公共依赖，导致安装后的 `CustomLogDependencies.cmake` 强制查找 Boost。
2. `core_platform_standalone` 显式安装完整 `smrobot_sdk_boost`，扩大了 SDK 交付面。
3. Business Source 默认从源码构建 `Common`，因此即使 Core/Platform 已预编译，仍需要 Boost 开发包。
4. Business Source 发布只复制仓库 `thirdparty`，默认排除了 SDK prefix 中的 `thirdparty`，使 FCL 等公共依赖继续依赖发布机的 `D:/PreBuild`。
5. Windows/Ubuntu 用户配置仍查找 PCL；顶层 `CMakeLists.txt` 仍无条件读取 `PCL::io` 以拼接调试 PATH。
6. Windows 用户配置仍保留已无所属模块的 LZ4/FLANN 条件查找。

## 4. 实施步骤

### 阶段 A：收紧 CustomLog 导出接口

1. 将 `Boost::log`、`Boost::log_setup` 以及其余 Boost 依赖全部调整为 `CustomLog` 私有链接依赖。
2. 为安装逻辑显式声明 `CustomLog` 没有 Boost 公共依赖，确保生成的 dependency resolver 不再执行 `find_dependency(Boost)`。
3. 检查安装后的 `CustomLogTargets*.cmake`；若仍存在导致 Windows 消费者必须创建 `Boost::*` target 的导出属性，在导出层修正，而不是要求消费者安装 Boost。
4. 保留 `function_InstallTarget` 对 `$<TARGET_RUNTIME_DLLS:CustomLog>` 的运行 DLL 安装；增加 PE 依赖闭包检查，避免把 Boost 开发包作为运行时修复手段。

### 阶段 B：收紧 SDK profile

1. 从 `core_platform_standalone` 移除 `smrobot_sdk_boost`。
2. 保留可选 `smrobot_sdk_boost` 安装组件，供内部完整工具链或源码构建 profile 使用；不删除通用安装能力。
3. 保留 FCL/OctoMap/libccd、Eigen、glm、nlohmann_json、URDFDOM 和 Verification SDK 等当前真实公共或运行依赖。
4. 确认 Assimp、KTX 等私有实现依赖只通过目标运行 DLL或静态链接闭包交付，不向消费者暴露开发包。

### 阶段 C：修订 Business Source 默认模式

1. 在导出包专属 `BusinessSourcePackageDefaults.cmake` 中增加 `UsingPrebuilt_Common=ON`。
2. 同步更新 `configure_business_prebuilt.ps1`、staging 验证参数和 `sdk.lock.json` 的 `usingPrebuilt` 清单。
3. 统一发布入口启用 `IncludeSdkThirdParty`，将 SDK prefix 的必要 thirdparty 内容复制到 Business Source 的 `PrebuiltPackages/thirdparty`。
4. 保留 Qt、OMPL 外部路径覆盖能力，不把它们加入 Core/Platform standalone profile。

### 阶段 D：删除遗留全局查找

1. 从所有当前 Windows/Ubuntu 用户配置删除 PCL 查找。
2. 删除顶层 `CMakeLists.txt` 中 `PCL::io`、PCL/VTK/OpenNI2 runtime PATH 拼接代码。
3. 从 Windows 用户配置删除 LZ4、FLANN 条件查找。
4. 不删除本机 `D:/PreBuild` 中的第三方文件；本轮只取消项目依赖和查找。

## 5. 验证门禁

1. PowerShell AST、CMake configure 与 `git diff --check` 通过。
2. 重新生成 Debug/Release `core_platform_standalone` SDK。
3. SDK prefix 不包含 `thirdparty/Boost` 的头文件、CMake Config 和 import libraries。
4. `Common/bin` 中包含 `CustomLog` 实际 PE 依赖闭包所需的 Boost Debug/Release DLL。
5. 在不提供 Boost CMake package/targets 的最小 consumer 中完成 `Common::CustomLog` configure、build 和运行。
6. 使用外部不同 Boost 版本的独立 consumer 与 `CustomLog` 同进程运行，确认没有公开 ABI 冲突。
7. 重新导出 Business Source，只指定本机用户即可自动导入 Prebuilt Common/Core/Platform。
8. Business Source Release 构建 `RobotQtViewer` 通过。
9. PCD ASCII、Binary、Binary Compressed 文件加载通过，并执行一次 RobotQtViewer PCD 导入 smoke。
10. 全仓搜索确认正式源码和配置不再引用 PCL/OpenNI2/LZ4/FLANN；历史文档引用不作为构建依赖。

## 6. 风险与停止条件

- 如果安装后的 `Common::CustomLog` 在 Windows consumer 中仍必须解析 `Boost::*` target，先修正导出 target 属性，不得重新加入完整 Boost SDK 作为掩盖。
- 如果 Business Source 的业务模块依赖未导出的 Common 组件，停止删除 Common 源码；优先补齐明确的 Common export component 或保留必要源码，不做隐式复制。
- 如果 PCD 三种编码中任一种依赖 PCL 才能正确读取，停止移除并记录缺失格式；不得降低现有点云能力。
- 如果 FCL 的公开类型仍出现在 Collision 公共头文件中，继续保留 FCL 开发包，直到独立封装任务完成。
- 不修改 Qt、OMPL、FCL 的外部 ABI 和版本策略；任何此类扩展另立计划。

## 7. 完成标准

- Core/Platform SDK 不再交付完整 Boost 开发包，`CustomLog` consumer 不需要 Boost package。
- Business Source 默认消费 Prebuilt Common/Core/Platform，并从包内获得 SDK 公共 thirdparty。
- Qt、OMPL 继续外部提供；FCL/OctoMap/libccd 临时随 SDK 提供。
- 当前产品 configure/build 不再依赖 PCL、OpenNI2、LZ4、FLANN。
- `RobotQtViewer` 构建和 PCD 导入能力保持不变。

## 8. 执行结果（2026-07-22）

- 已完成 `CustomLog` Boost 公共依赖移除，生成的 `CustomLogDependencies.cmake` 不再解析 Boost。
- 已从 `core_platform_standalone` 移除完整 Boost 开发包；SDK 仅保留目标运行闭包复制出的 Debug/Release Boost DLL。
- 已让 Business Source 默认使用 Prebuilt `Common`、`SMRobotCore`、`SMRobotPlatform`，并复制 SDK thirdparty。
- 已修复 `Common::GLRuntime` 的 GLAD 公共头文件安装组件，避免 `RenderCore` 下游缺少 `glad/glad.h`。
- 已从当前 Windows/Ubuntu 用户配置及顶层运行时配置移除 PCL/OpenNI2、LZ4、FLANN 查找。
- 4090 + VS2019 x64 配置通过；SDK Debug/Release 各 21 项 consumer matrix 全部通过。
- Business Source Release `RobotQtViewer` 构建通过。
- `SensorSimulation-PointCloudLoadExample` 构建并运行通过，ASCII PCD `bunny.pcd` 成功加载 397 个点。
- OMPL 仍按设计使用外部 Boost；Qt 仍按设计由用户环境提供；FCL/OctoMap/libccd 继续作为 SDK 临时公共第三方依赖。
