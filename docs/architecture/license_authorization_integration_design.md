# RS2026 授权认证接入设计

## 目标

为当前机器人仿真平台接入 `VerificationDeveloperKit-1.0.0` 授权 SDK，使核心能力在运行时检查本机 license 是否满足对应授权项。

本轮已进入实现阶段，包含授权适配层、Core/Platform 授权门面、运行时门槛和构建配置。

## SDK 调研结论

授权 SDK 位于 `thirdparty/Windows/VerificationDeveloperKit-1.0.0`，对外公开内容包括：

- `include/LicenseSystem/LicenseClient.h`
- `include/LicenseSystem/LicenseTypes.h`
- `lib/cmake/VerificationSDK/VerificationSDKConfig.cmake`
- Debug/Release 静态库：`VerificationLicenseSDK.lib`、`VerificationLicense.lib`、`VerificationHardwareInfo.lib`、`VerificationEncryption.lib`、`VerificationOpenSSLCrypto.lib` 等
- 工具：`InfoCollection_rx64.exe`、`VerificationLicenseCheck_rx64.exe`
- 示例：`Examples/EmbeddedLicenseClient`

SDK 的接入方式是：

```cmake
find_package(VerificationSDK 1.0 CONFIG REQUIRED)
target_link_libraries(<target> PRIVATE LicenseSystem::LicenseSDK)
```

主要 C++ API 是：

```cpp
LicenseSystem::LicenseClient client(productId);
client.setSearchOptions(options);
client.addSearchDirectory(directory);
client.setLicenseFileNames(fileNames);
auto result = client.authenticateCurrentMachine();
auto result = client.authenticateLicenseFile(path);
bool enabled = client.isFeatureEnabled(featureName);
```

SDK 自带搜索选项支持可执行目录、可执行目录下 `license/`、工作目录和额外搜索目录。我们的需求包含固定目录和 license 目录递归搜索，因此需要在项目内封装一层搜索策略，不能只依赖 SDK 默认搜索。

## 授权身份映射

SDK 的 `productId` 是一层字符串，不直接表达 Project/Module/Feature 三层。为避免和 `SimulationProject` 的项目文档概念混淆，本项目统一约定：

```text
SDK productId: RS2026
Module:        SMRobot
Feature:       RobotCore 或 RobotPlatform
```

推荐 license 中的 feature 字符串使用点分命名：

```text
SMRobot.RobotCore
SMRobot.RobotPlatform
```

授权检查规则：

- `SMRobotCore` 负责验证 `RS2026` 产品下 `SMRobot` 模块的 `RobotCore` feature。
- `SMRobotPlatform` 负责验证 `RS2026` 产品下 `SMRobot` 模块的 `RobotPlatform` feature。
- `RS2026(Project)` 与 `RS2026(Product)` 在授权语义上归一为 SDK `productId = "RS2026"`；项目文件里的 `SimulationProject` 不承担授权身份定义。

如果现有签发系统已经使用扁平 feature 名，例如 `RobotCore`，实现阶段可以临时兼容别名，但新签发 license 应只使用 `SMRobot.RobotCore` 和 `SMRobot.RobotPlatform`。

## 设计不变量

授权检查是运行时能力边界，不是 GUI 状态、项目文件状态或单个按钮状态。

不变量：

- 没有有效 `RS2026` license 时，不允许使用受保护能力。
- license 有效但缺少 `SMRobot.RobotCore` 时，不允许使用 `SMRobotCore` 的受保护 RobotCore 能力。
- license 有效但缺少 `SMRobot.RobotPlatform` 时，不允许使用 `SMRobotPlatform` 的受保护 RobotPlatform 能力。
- license 搜索路径必须集中维护，不能由 Core、Platform、Viewer、示例程序各自拼路径。
- license 内容和 SDK `diagnosticsJson` 不应完整写入普通日志。

## 拟新增组件

建议新增一个小的非 UI 授权适配层，避免 SDK 头文件和搜索策略扩散到业务模块。

```text
Common::LicenseVerification
    通用 Verification SDK 适配层
    负责 license 搜索、SDK 调用、结果缓存、状态转换、日志脱敏

SMRobotCore::RobotCoreAuthorization
    Core 授权门面
    固定 requirement = RS2026 / SMRobot.RobotCore
    供 RobotCore、RobotSDK、RobotIO/Runtime 入口调用

SMRobotPlatform::RobotPlatformAuthorization
    Platform 授权门面
    固定 requirement = RS2026 / SMRobot.RobotPlatform
    供 SimulationProject、SimulationRuntime、ViewerCore 等平台入口调用
```

`Common::LicenseVerification` 只表达通用 license 验证能力，不硬编码 RS2026 业务身份。RS2026 的 product/module/feature 常量由 Core/Platform 授权门面持有。

## 拟新增主要类型

```cpp
namespace common::license
{
struct LicenseRequirement
{
    std::string productId;
    std::string moduleId;
    std::string featureId;
    std::string canonicalFeatureName;
    std::vector<std::string> acceptedFeatureAliases;
};

enum class AuthorizationStatus
{
    Authorized,
    LicenseFileNotFound,
    InvalidLicense,
    ProductMismatch,
    FeatureNotAllowed,
    HardwareMismatch,
    Expired,
    InternalError
};

struct AuthorizationResult
{
    AuthorizationStatus status;
    std::string message;
    std::filesystem::path licenseFile;
    std::vector<std::filesystem::path> searchedPaths;
};

class LicenseVerifier
{
public:
    AuthorizationResult verify(const LicenseRequirement& requirement);
};
}
```

Core 门面：

```cpp
namespace smrobotcore::authorization
{
const common::license::AuthorizationResult& ensureRobotCoreAuthorized();
bool isRobotCoreAuthorized();
}
```

Platform 门面：

```cpp
namespace smrobotplatform::authorization
{
const common::license::AuthorizationResult& ensureRobotPlatformAuthorized();
bool isRobotPlatformAuthorized();
}
```

`ensure*Authorized()` 在失败时返回失败结果或抛出项目内已有错误类型，具体错误机制在实现阶段按现有模块风格确定。对 SDK/示例更友好的接口保留 `bool` 和结果对象。

## License 查找策略

当前仓库已有 `license/license.LIC`。实现时搜索策略统一由 `Common::LicenseVerification` 维护。

基础候选目录按以下顺序构建：

1. 可执行程序目录。
2. 可执行程序目录下的 `license/`。
3. 可执行程序目录下的 `licenses/`。
4. 源代码根目录下的 `license/`，仅源码构建或测试构建启用。
5. 当前工作目录。
6. 当前工作目录下的 `license/`。
7. 当前工作目录下的 `licenses/`。
8. 调用方通过 API、环境变量或命令行显式追加的目录。

对所有名为 `license` 或 `licenses` 的候选目录，递归遍历其全部子目录。递归结果去重并保持稳定顺序。

候选文件策略：

- 优先检查 `license.LIC`。
- 再检查 `RS2026.LIC`、`rs2026.LIC`。
- 最后枚举候选目录中所有 `.LIC` 和 `.lic` 文件。

因为 SDK 的 `setLicenseFileNames()` 不提供通配枚举，项目封装层应直接枚举候选 license 文件，并对每个文件调用：

```cpp
LicenseClient("RS2026").authenticateLicenseFile(candidate)
```

停止条件：

- 找到 product 匹配且 feature 满足的 license，立即返回 `Authorized`。
- 找到 product 匹配但 feature 不满足的 license，记录 `FeatureNotAllowed`，继续检查后续 license；如果最后没有更高优先级有效 license，则返回该状态。
- 所有候选都失败且没有文件存在，返回 `LicenseFileNotFound`。
- 所有候选都失败但存在 license 文件，返回最有诊断价值的失败状态，例如 `Expired`、`HardwareMismatch`、`ProductMismatch`。

## CMake 接入设计

源码构建阶段：

```cmake
set(VerificationSDK_DIR
    "${PROJECT_SOURCE_DIR}/thirdparty/Windows/VerificationDeveloperKit-1.0.0/lib/cmake/VerificationSDK"
)
find_package(VerificationSDK 1.0 CONFIG REQUIRED)
```

新增目标链接：

```cmake
target_link_libraries(LicenseVerification
    PRIVATE
        LicenseSystem::LicenseSDK
)
```

`LicenseSystem::LicenseSDK` 不应出现在 Core/Platform 公开头文件中，因此保持 `PRIVATE` 依赖。公开 API 使用项目自己的 `AuthorizationResult` 等类型。

安装/预编译包阶段：

- 将已内嵌 production public key 与 AES key ring 的 `VerificationDeveloperKit-1.0.0` 作为 SDK 第三方闭包的一部分发布，或在 `SMRobotDependencyBootstrap.cmake` 中从安装前缀恢复 `VerificationSDK_DIR`。
- `SMRobotCore` 与 `SMRobotPlatform` 的 installed package config 必须能在外部 consumer 中解析授权组件依赖。
- 不安装生产私钥、LicenseGenerator、`licenses/keys/production` 或内部签发配置。
- `licenses/keys/production/key-management.keymgmt.json` 只允许作为 Verification SDK 生成阶段的输入，通过 `LICENSESYSTEM_EMBEDDED_KEY_CONFIG_FILE` 烧录到 SDK 库和工具中；运行时和对外 SDK 不从该目录读取明文 key。
- `license/license.LIC` 不是 SDK 必备依赖；正式交付时由最终用户部署授权方签发的 license。

## 运行时接入点

Core 层建议接入点：

- `RobotSDK` 对外入口初始化时验证 `SMRobot.RobotCore`。
- `RobotIO` 加载机器人模型前验证 `SMRobot.RobotCore`。
- `RobotRuntime` 创建机器人运行时实例或执行轨迹前验证 `SMRobot.RobotCore`。
- `RobotCore` 中低层纯数据类型不宜在每个 getter/setter 上重复验证。

Platform 层建议接入点：

- `SimulationProject` 打开、保存或构建项目会话前验证 `SMRobot.RobotPlatform`。
- `SimulationRuntime` 从项目构建运行时前验证 `SMRobot.RobotPlatform`。
- `RobotViewerCore` 或上层应用只消费 Platform/Core 的授权结果，不直接调用 `LicenseSystem::LicenseClient`。

授权结果应进程内缓存。默认每个 requirement 首次验证一次；如果未来需要 license 热更新，再添加显式 `refreshAuthorization()`，不要在每帧渲染或每次查询中访问文件系统。

## 失败语义和用户反馈

失败信息分两层：

- 工程日志：记录状态、脱敏后的 license 路径、搜索路径数量、product/feature 名称。
- UI/CLI：显示简洁可操作信息，例如缺少 license、license 不属于本机、license 已过期、未授权 RobotCore/RobotPlatform feature。

不要在普通日志中输出：

- license 文件内容。
- `diagnosticsJson` 全量内容。
- 硬件指纹原文。
- SDK 内部密钥相关信息。

## 与现有架构的关系

该设计不改变现有机器人、项目、场景、碰撞和 Workbench 所有权：

- 授权 SDK 适配归 `Common::LicenseVerification`。
- `SMRobotCore` 只拥有 RobotCore feature 的授权门面和调用点。
- `SMRobotPlatform` 只拥有 RobotPlatform feature 的授权门面和调用点。
- `RobotQtViewer`、Workbench、Dialog 不拥有授权语义，只展示授权失败结果。
- 项目文件不保存授权状态，也不因授权失败被修改。

## 实施阶段建议

阶段 A：授权适配层

- 新增 `Common::LicenseVerification`。
- 接入 `VerificationSDK` CMake 查找。
- 确认 `VerificationDeveloperKit-1.0.0` 是使用 `licenses/keys/production/key-management.keymgmt.json` 重新生成的 embedded-key 版本。
- 实现 license 候选目录、递归枚举、结果转换和缓存。
- 新增 headless smoke test，验证从 `license/license.LIC` 找到 license。

阶段 B：Core feature 门面

- 新增 `SMRobotCore::RobotCoreAuthorization`。
- 常量：`productId = "RS2026"`，`canonicalFeatureName = "SMRobot.RobotCore"`。
- 在 `RobotSDK`、`RobotIO`、`RobotRuntime` 的高层入口接入。
- 增加缺失 license、缺少 feature、有效 license 三类测试。

阶段 C：Platform feature 门面

- 新增 `SMRobotPlatform::RobotPlatformAuthorization`。
- 常量：`productId = "RS2026"`，`canonicalFeatureName = "SMRobot.RobotPlatform"`。
- 在 `SimulationProject`、`SimulationRuntime` 的高层入口接入。
- Viewer 只把失败信息转成状态栏或启动错误提示。

阶段 D：安装与预编译闭包

- 将 Verification Developer Kit 纳入 installed SDK 依赖恢复。
- 验证 source、Core prebuilt、Core+Platform prebuilt、Workbench/App source 混合构建矩阵。
- 用外部 consumer 验证不需要手动设置 `VerificationSDK_DIR`。

## 验证计划

静态检查：

```bat
rg "LicenseSystem::LicenseClient" Common SMRobotCore SMRobotPlatform SMRobotApps SimWorkbench
```

预期只有 `Common::LicenseVerification` 直接包含 SDK 头文件。

构建验证：

```bat
cmake -S . -B build
cmake --build build --config Release --target RobotQtViewer
cmake --install build --config Release --prefix build\install
```

授权 smoke test：

```bat
cmake --build build --config Release --target LicenseAuthorizationSmokeTest
ctest --test-dir build -C Release -R LicenseAuthorization --output-on-failure
```

外部 consumer：

```bat
cmake -S tests\package_examples\license_authorization_consumer -B build\license_authorization_consumer -DCMAKE_PREFIX_PATH="<repo>\build\install"
cmake --build build\license_authorization_consumer --config Release
build\license_authorization_consumer\Release\LicenseAuthorizationConsumer.exe
```

手动工具核验：

```bat
thirdparty\Windows\VerificationDeveloperKit-1.0.0\Tools\VerificationLicenseCheck\Release\VerificationLicenseCheck_rx64.exe license\license.LIC RS2026
```

具体工具参数以 `--help` 输出为准。

## 风险和待确认项

- 需要确认授权方签发系统最终使用的 feature 字符串是否为 `SMRobot.RobotCore` / `SMRobot.RobotPlatform`。如果不是，应在实现前固定映射。
- 需要确认 `VerificationLicenseCheck_rx64.exe` 是否能直接检查 feature；如果不能，feature 检查以 SDK smoke test 为准。
- 当前 Developer Kit 若由无 embedded key 配置生成，会返回 `No embedded public key is configured.`。这类错误不能通过运行时传 key 路径修复，必须重新生成带 embedded key 的 Developer Kit。
- 源码根目录 `license/` 只适合开发和 CI，不应作为正式交付唯一部署位置。
- 如果后续要求运行时替换 license，需要新增显式刷新 API，并定义缓存失效规则。
- 如果要求完全禁止未授权模块被链接或加载，仅运行时检查不够，需要另行设计包级分发和动态加载策略。

## 发布/开发授权模式

运行时门槛由 CMake cache 变量 `RS2026_LICENSE_ENFORCEMENT_MODE` 控制：

```text
Enforce
ReportOnly
Disabled
```

- `Enforce` 是默认值，也是正式发布唯一允许的模式。授权失败时，`RobotCore` 与 `RobotPlatform` 的受保护能力拒绝继续执行。
- `ReportOnly` 只允许开发、Codex 和 CI 诊断构建使用。该模式仍执行授权验证并输出失败诊断，但不会阻止程序继续运行。
- `Disabled` 只允许本地临时排障使用。该模式跳过运行时门槛，不得用于发布包、演示包或客户交付包。

发布流水线不得依赖开发者本地 CMake cache，应显式配置：

```bat
cmake -S . -B build_release -DRS2026_LICENSE_ENFORCEMENT_MODE=Enforce
```

Codex 或受限 CI 环境需要运行 viewer/profile 时，可以显式配置：

```bat
cmake -S . -B build -DRS2026_LICENSE_ENFORCEMENT_MODE=ReportOnly
```

该开发后门只改变 `require*Authorization()` 的失败处理，不改变 `verify*Authorization()` 的真实验证结果。
