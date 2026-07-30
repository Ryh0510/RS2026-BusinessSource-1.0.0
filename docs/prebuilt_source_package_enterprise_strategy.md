# 预编译 Core/Platform 与业务源码包的企业级交付策略

## 本文目标

本文只分析方案，不修改代码。

重点回答三个问题：

1. `thirdparty` 已经作为子仓库，是否还需要单独版本化或其他机制。
2. 直接使用现有架构：先全量编译/install 到 `PrebuiltPackages`，再删除 `SMRobotCore` / `SMRobotPlatform` 源码并设置 `UsingPrebuilt_*`，是否可行。
3. 这种做法与“Core/Platform 二进制 SDK + 业务源码包”方案有什么不同，企业级开发通常怎么做。

`fcl` 本文暂不讨论。

## 当前代码机制判断

当前仓库已经具备“源码包混合预编译包”的基础能力。

已有机制包括：

- `.gitmodules` 中 `thirdparty` 是独立子仓库：

```text
path = thirdparty
url = gitea@123.207.55.50:tangtang_dev/RSThirdParty.git
branch = main
```

- `cmake/ProjectPackagesConfigSetting.cmake` 中有：
  - `SMROBOT_PREBUILT_PACKAGE_ROOT`
  - `SMROBOT_PREBUILT_PACKAGE_SELECTION_MODE`
  - `UsingPrebuilt_<Package>`
  - `BuildPackage_<Package>`
  - `SMRobotCore_PrebuiltComponents`
  - `SMRobotPlatform_PrebuiltComponents`

- `cmake/FindPrebuiltPackages.cmake` 和 `cmake/FuncDef_PackageInstall.cmake` 提供 `find_prebuilt_package()`。

- 顶层 `CMakeLists.txt` 的主包加载顺序中，遇到 `UsingPrebuilt_${PACKAGE_NAME}` 会先 `rs_find_requested_prebuilt_package()`，然后不再构建源码组件。

- `SMRobotCore/CMakeLists.txt` 和 `SMRobotPlatform/CMakeLists.txt` 中，当 `UsingPrebuilt_${PACKAGE_NAME}` 为 ON 时，会打印“source components are not added”，不会进入各 component 源码目录。

因此，你提出的路线在当前架构下是成立的：

```text
完整源码仓库
  -> 构建并 install Core/Platform 到 PrebuiltPackages 或 SDK prefix
  -> 生成业务源码包时删除 SMRobotCore / SMRobotPlatform 源码目录
  -> CMake 配置 UsingPrebuilt_SMRobotCore=ON / UsingPrebuilt_SMRobotPlatform=ON
  -> 上层业务源码继续开发
```

这不是错路。它实际上是“源码分发包 + 预编译底座”的一种实现。

## thirdparty 子仓库是否足够

`thirdparty` 做成子仓库是正确的，但它只解决了一部分问题。

### 子仓库已经解决的问题

子仓库适合解决：

- 统一第三方源码/预编译依赖目录；
- 主仓库锁定某个 thirdparty commit；
- 团队可以用同一个 thirdparty 版本构建；
- thirdparty 更新可以独立提交、独立回滚；
- 主仓库不需要把 thirdparty 历史全部混在一起。

这对源码开发非常有价值。

### 子仓库没有自动解决的问题

子仓库不自动解决：

- install SDK 时仍然复制大量 thirdparty 文件；
- 下游是否拿到了正确的 thirdparty commit；
- 下游是否需要完整 clone 子仓库；
- thirdparty 二进制是否已经构建好；
- Debug/Release、VS 版本、运行时库是否匹配；
- SDK artifact 与 thirdparty artifact 的版本关系；
- 多个业务包是否重复携带同一份 Boost/Qt/Assimp 等内容；
- 构建机是否能无网络、可复现地拿到同一套 thirdparty。

也就是说，子仓库是“源码版本管理机制”，不是完整的“二进制依赖发布机制”。

### 成熟建议

更成熟的做法通常是两层：

```text
thirdparty 源码/构建仓库
  -> 产生 thirdparty binary artifact
      -> Core/Platform SDK 和业务源码包都引用该 artifact 版本
```

可以继续保留当前 `thirdparty` 子仓库，但建议增加一个独立的 thirdparty 发布概念：

```text
SMRobotThirdParty-vs2019-x64-2026.07/
  manifest.json
  Boost/
  Qt5/
  assimp/
  KTX/
  Eigen340/
  glm/
  nlohmann_json/
  Windows/
```

`manifest.json` 至少记录：

- thirdparty git commit；
- 编译器版本；
- Windows SDK；
- CMake 版本；
- Debug/Release 是否齐全；
- 运行时库策略，例如 `/MD`；
- 各依赖版本；
- artifact hash。

这样 Core/Platform SDK 小改时，只发布新的 `SMRobotSDK`，不重新发布 `SMRobotThirdParty`。只有 thirdparty 真的变化时，才发布新的 thirdparty artifact。

## 你的方法是否可行

可行，而且应该作为第一阶段最小落地路线。

你的方法可以定义为：

```text
Monorepo Trimmed Source Distribution
```

即从完整仓库生成一个裁剪版源码包：

```text
RSBusinessDev/
  CMakeLists.txt
  Common/                       # 视情况保留源码或预编译
  SMRobotSpray/                 # 业务源码
  SMRobotApps/                  # 应用源码
  SimWorkbench/                 # 业务 workbench 源码
  SMRobotMotionPlanning/        # 如果需要业务侧继续开发则保留
  PrebuiltPackages/             # 或外部 SDK prefix
    SMRobotCore/
    SMRobotPlatform/
  cmake/
  docs/
```

删除：

```text
SMRobotCore/
SMRobotPlatform/
```

配置：

```powershell
cmake -S RSBusinessDev -B build\business `
  -DUSER_TANGTANG_p15v3_Windows=ON `
  -DSMROBOT_PREBUILT_PACKAGE_ROOT=D:\path\to\PrebuiltPackages `
  -DUsingPrebuilt_SMRobotCore=ON `
  -DUsingPrebuilt_SMRobotPlatform=ON
```

从当前 CMake 看，这条路已有机制支撑。

## 你的方法与建议方法的区别

两者目标一致，差别不在“能不能构建”，而在交付治理方式。

### 你的方法

```text
完整仓库
  -> install 到 PrebuiltPackages
  -> 删除 Core/Platform 源码
  -> 设置 UsingPrebuilt
  -> 交付裁剪仓库
```

优点：

- 最贴合当前仓库结构；
- 改造成本最低；
- 上层业务源码几乎不用搬家；
- 可以很快验证“没有 Core/Platform 源码也能构建”；
- 对团队内部开发很友好。

风险：

- 如果靠手工删除目录，容易漏删、误删或交付不一致；
- 如果 `SMRobotCore/SMRobotPlatform` 源码仍在，可能出现“其实还是从源码编译”的假通过；
- `PrebuiltPackages` 如果放在源码包里，会让源码包变大；
- 每次发布可能重复携带 thirdparty；
- SDK 版本、thirdparty 版本、业务源码版本之间容易只靠口头约定；
- 下游分支继续开发后，和完整源码主线的关系容易变成“半个仓库 fork”；
- 如果业务源码无意 include 了 Core/Platform private headers，只有删除源码后才暴露问题；
- 业务源码包仍深度耦合主仓库的顶层 CMake 扫描逻辑和目录命名。

### 建议方法

```text
完整仓库
  -> 自动生成 Core/Platform SDK artifact
  -> 自动生成 Business Source Package
  -> Business Source Package 显式依赖 SDK artifact 和 ThirdParty artifact
```

它不是反对你的路线，而是把你的路线产品化：

- 删除源码变成脚本规则，不靠手工；
- `PrebuiltPackages` 变成版本化 artifact，不靠某个目录当前状态；
- `thirdparty` 变成单独 artifact，不随每次 SDK 小改重复复制；
- 业务源码包有自己的 manifest，记录它依赖哪个 SDK/thirdparty；
- 每个交付包有 smoke test；
- 可以在 Gitea、内网构建机或本地一键脚本中重复生成。

关键差异如下：

| 维度 | 直接删目录 + PrebuiltPackages | 自动生成 SDK + 业务源码包 |
| --- | --- | --- |
| 初始落地速度 | 快 | 稍慢 |
| 是否贴合现有 CMake | 很贴合 | 需要包装现有 CMake |
| 可复现性 | 取决于人工流程 | 由脚本和 manifest 保证 |
| thirdparty 更新 | 容易重复携带 | 可独立版本化 |
| 分支长期维护 | 容易变成裁剪仓库 fork | 更像发布产物，不承担主线合并 |
| 对第三方 AI 友好度 | 取决于文档 | 可附带专门 AI_CONTEXT |
| 企业级审计 | 较弱 | 较强 |
| 适合阶段 | 第一阶段验证、内部开发 | 稳定交付、多人/多团队复用 |

## 企业级开发通常怎么做

C++ 企业级项目常见有几种成熟形态。

### 形态 A：源码主仓库 + 二进制 SDK artifact

这是最常见的 SDK 发布模型。

特点：

- 源码只在核心团队仓库中；
- 对外发布 install 后的 headers/libs/DLL/CMake config；
- 下游通过 `find_package` 使用；
- artifact 存在 Gitea Release、共享盘、Nexus、Artifactory、NuGet、Conan remote 或内网对象存储中；
- 每个 artifact 有版本号和 hash。

适合：

- 真正对外部团队或客户发布 SDK；
- API/ABI 需要治理；
- 不希望下游接触主仓库构建复杂度。

### 形态 B：裁剪源码包 + 预编译底座

这最接近你当前想做的事。

特点：

- 保留上层业务源码；
- 删除或不分发底层源码；
- 底层通过 `UsingPrebuilt` 或 `find_package` 引入；
- 下游仍然可以开发业务源码；
- 裁剪源码包由脚本生成。

适合：

- 内部团队协作；
- 业务层还没成熟，需要继续源码开发；
- 底层平台团队希望保护 Core/Platform 源码。

企业里会做，但通常会要求：

- 不手工删除，必须脚本生成；
- 生成后必须在无 Core/Platform 源码环境中 configure/build；
- 业务源码包要带 `sdk.lock.json`；
- `PrebuiltPackages` 要有版本号；
- 不允许源码包自动回退到本地源码。

### 形态 C：包管理器/二进制缓存

常见工具：

- Conan；
- 外部依赖管理器的 binary cache；
- NuGet；
- Artifactory/Nexus；
- Gitea Release + 自定义 CMake 脚本；
- 内网共享目录 + manifest。

它们解决的是：

- 二进制依赖复用；
- 版本锁定；
- 缓存；
- 多工程一致性；
- 不重复复制大依赖。

对当前项目，不一定马上引入外部依赖管理器。可以先用 Gitea Release 或共享目录模拟 artifact 仓库。

## 推荐改进方案

### 第一阶段：保留你的路线，但脚本化

目标：不改变架构，只把手工步骤变成可重复命令。

建议新增一个“业务源码包生成流程”，概念上做：

```text
1. 构建完整源码。
2. install Core/Platform SDK profile。
3. 复制源码仓库到 staging。
4. 删除 staging 中的 SMRobotCore/SMRobotPlatform。
5. 删除 staging 中不该交付的 build/cache/内部文档。
6. 写入 sdk.lock.json。
7. 写入默认 CMake 配置，例如 UsingPrebuilt_SMRobotCore=ON。
8. 在 staging 中重新 configure/build。
9. 打 zip。
```

`sdk.lock.json` 示例：

```json
{
  "businessPackage": "SprayBusinessDev",
  "businessVersion": "0.1.0",
  "sourceCommit": "<main repo commit>",
  "sdk": {
    "name": "SMRobotSDK",
    "version": "1.4.0",
    "commit": "<sdk source commit>",
    "pathHint": "PrebuiltPackages",
    "hash": "<artifact hash>"
  },
  "thirdparty": {
    "name": "SMRobotThirdParty",
    "gitCommit": "<thirdparty submodule commit>",
    "artifact": "SMRobotThirdParty-vs2019-x64-2026.07",
    "hash": "<artifact hash>"
  }
}
```

这一阶段，你的方法就是主方案。

### 第二阶段：拆出 thin SDK profile

目标：避免每次 SDK 小改都复制 thirdparty。

建议保留两个 profile：

- `core_platform_standalone`：完整离线包，包含 thirdparty。
- `core_platform_thin_internal`：内部开发包，不复制 thirdparty，只记录依赖。

内部业务团队默认用 thin profile：

```text
CMAKE_PREFIX_PATH =
  D:\sdk\SMRobotSDK-1.4.0-thin;
  D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07
```

正式对外或离线客户再用 standalone profile。

### 第三阶段：建立 artifact 仓库

目标：不要把版本化大二进制长期塞进业务源码分支。

最小可行方案：

```text
\\server\SMRobotArtifacts\
  SMRobotSDK\
    1.4.0\
      SMRobotSDK-1.4.0-thin.zip
      SMRobotSDK-1.4.0-standalone.zip
      manifest.json
  SMRobotThirdParty\
    vs2019-x64-2026.07\
      SMRobotThirdParty-vs2019-x64-2026.07.zip
      manifest.json
  BusinessSourcePackage\
    spray-dev-0.1.0\
      SprayBusinessDev-0.1.0.zip
      sdk.lock.json
```

如果使用 Gitea，可以用 Gitea Release 或 Package Registry 承载这些 zip。没有云 CI 也没关系，本地脚本生成后上传即可。

### 第四阶段：加边界验证

目标：确认业务源码包真的没有偷偷依赖 Core/Platform 源码。

建议每次生成业务源码包后执行：

```text
configure without SMRobotCore/SMRobotPlatform source
build selected business targets
run package smoke tests
scan include paths for deleted source directories
scan CMake link targets for private/internal components
```

这比“手动看能不能编译”更可靠。

## 对当前路线的建议结论

你提出的路线可以作为第一阶段：

```text
install 到 PrebuiltPackages
删除 Core/Platform 源码
UsingPrebuilt_SMRobotCore=ON
UsingPrebuilt_SMRobotPlatform=ON
交付业务源码包
```

但不要把它做成长期手工流程。成熟做法是把它升级为：

```text
可重复生成的裁剪源码包
  + 明确版本的 Core/Platform SDK artifact
  + 明确版本的 ThirdParty artifact
  + sdk.lock.json
  + 无源码环境验证
```

换句话说：

- 你的方法是正确的工程入口；
- 我建议的是企业级交付包装；
- 两者不是互斥关系；
- 第一阶段应该沿用你现有架构，先做自动化和验证，而不是推翻重来。

## 推荐第一步

第一步建议不是重构 CMake，也不是立刻引入外部依赖管理器。

第一步应该定义并实现一个“业务源码包生成规范”：

1. 明确哪些目录保留、哪些目录删除。
2. 明确 SDK prefix 放在哪里。
3. 明确 thirdparty 使用子仓库还是独立 artifact。
4. 生成 `sdk.lock.json`。
5. 生成后在干净 staging 目录重新 configure/build。

只要这个闭环跑通，你当前架构就已经可以支持“Core/Platform 二进制 + 业务源码继续开发”的目标。

## 第一阶段脚本入口

当前第一阶段脚本入口为：

```text
scripts/package_business_source.ps1
```

它的职责是把上面的人工流程包装成可重复命令：

1. 复制当前源码树到 staging。
2. 删除 staging 中的 `SMRobotCore` 和 `SMRobotPlatform`。
3. 复制指定 SDK prefix 到 staging 中的 `PrebuiltPackages`。
4. 默认不复制 SDK prefix 里的 `thirdparty`，而是保留业务源码包根目录的 `thirdparty` 子仓库内容。
5. 写入 `sdk.lock.json`。
6. 写入 `configure_business_prebuilt.ps1`，方便下游一条命令配置。
7. 可选执行 staging configure/build 验证。
8. 可选生成 zip。

推荐先用本地已安装好的 SDK prefix 生成包：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.1.0 `
  -SdkPrefix build\sdk_local_install `
  -AllowDirtyTree `
  -CleanStaging
```

如果希望在生成后验证裁剪包能配置：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.1.0 `
  -SdkPrefix build\sdk_local_install `
  -AllowDirtyTree `
  -CleanStaging `
  -VerifyConfigure
```

如果希望同时构建某个业务目标：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.1.0 `
  -SdkPrefix build\sdk_local_install `
  -AllowDirtyTree `
  -CleanStaging `
  -VerifyBuild `
  -VerifyBuildTarget RobotQtViewer
```

生成后的下游使用方式：

```powershell
cd build\business_source_staging\SprayBusinessDev-0.1.0
powershell -ExecutionPolicy Bypass -File configure_business_prebuilt.ps1 -Build -BuildTarget RobotQtViewer
```

当前脚本的边界：

- 不删除当前源码仓库中的 `SMRobotCore` / `SMRobotPlatform`，只删除 staging 副本。
- 不默认复制 SDK 里的 `thirdparty`，避免和业务源码包根目录的 `thirdparty` 重复。
- 如果需要做完全离线包，可以加 `-IncludeSdkThirdParty`。
- 如果 staging 目录已存在，必须显式传 `-CleanStaging` 才会清理。
- 当前仍依赖现有 CMake 的 `UsingPrebuilt_SMRobotCore` / `UsingPrebuilt_SMRobotPlatform` 机制，不改变主工程架构。

本次落地验证结果：

- 已生成业务源码 staging：`build/business_source_staging/SprayBusinessDev-0.1.0`。
- 已生成业务源码压缩包：`build/business_source_artifacts/SprayBusinessDev-0.1.0.zip`。
- zip SHA256：`52aaae0a40681bb1c62d2aa3744e3f41aac8a5cf4a51828eeed2ef3037d3129d`。
- staging 根目录下 `SMRobotCore` / `SMRobotPlatform` 均不存在。
- staging 中 `PrebuiltPackages/SMRobotCore` / `PrebuiltPackages/SMRobotPlatform` 均存在。
- 默认未复制 `PrebuiltPackages/thirdparty`，thirdparty 仍走业务源码包根目录的子仓库内容。
- 已在 staging 中执行 `configure_business_prebuilt.ps1 -Build -BuildTarget SprayCore`，configure 成功，`SprayCore` Release 构建成功。
- 旧 staging 清理时，如果 CMake 生成目录在 Windows 上出现瞬时路径/属性问题，脚本会先把旧目录移动为 `.deleting-*` 后继续生成新包；该目录不进入 zip，可后续单独清理 `build/business_source_staging`。

## 第二阶段脚本入口

第二阶段把 Core/Platform SDK 分成两个 profile：

- `core_platform_thin_internal`：内部开发默认 profile，不安装 `thirdparty` 目录，不安装 `data`，只安装 Core/Platform/Common 的 SDK 二进制、头文件和 CMake config。
- `core_platform_standalone`：完整离线 profile，保留原先完整复制 thirdparty 相关组件的行为。

旧 profile `core_platform_dll_e_final` 保留，语义上等价于历史完整 SDK；新脚本默认使用 `core_platform_thin_internal`。

内部 thin SDK 构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 `
  -Profile core_platform_thin_internal `
  -SdkPrefix build\sdk_thin_internal `
  -AllowDirtyTree `
  -CleanInstall
```

完整离线 SDK 构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 `
  -Profile core_platform_standalone `
  -SdkPrefix build\sdk_standalone `
  -AllowDirtyTree `
  -CleanInstall
```

业务源码包第二阶段默认也不复制 `thirdparty` 和 `data`。如果这些目录已经由团队作为独立子仓库或 artifact 提供，下游 configure 时显式指定即可：

```powershell
cd build\business_source_staging\SprayBusinessDev-0.1.0
powershell -ExecutionPolicy Bypass -File configure_business_prebuilt.ps1 `
  -ThirdPartyRoot D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07 `
  -DataRoot D:\sdk\SMRobotData-2026.07 `
  -Build `
  -BuildTarget SprayCore
```

生成业务源码包时也可以把默认路径提示写进 `sdk.lock.json`：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.2.0 `
  -SdkPrefix build\sdk_thin_internal `
  -ThirdPartyRootHint D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07 `
  -DataRootHint D:\sdk\SMRobotData-2026.07 `
  -AllowDirtyTree `
  -CleanStaging
```

第二阶段的边界：

- `thirdparty` 和 `data` 不再默认进入 thin SDK 或业务源码包。
- `build` 仍然是生成目录，不进入业务源码包。
- 如果临时需要完整业务包，可以显式加 `-IncludeThirdParty` / `-IncludeData`。
- 如果需要 SDK 自身完整离线，可以使用 `core_platform_standalone` 或业务包脚本的 `-IncludeSdkThirdParty`。
- CMake 新增 `SMROBOT_THIRDPARTY_ROOT` / `SMROBOT_DATA_ROOT`，默认仍指向源码树内目录，所以完整源码开发路径不变。

本次第二阶段落地验证结果：

- 已生成 thin SDK：`build/sdk_thin_internal`。
- thin SDK 根目录不包含 `thirdparty` / `data` / `build`。
- thin SDK manifest 中 `profile` 为 `core_platform_thin_internal`，`dependency_policy.mode` 为 `external`。
- thin SDK 已生成 export snapshot，hash 为 `f1ceb447a05b9e75cf8ace507e231c8a30034cb421f2498a09102d524dcf5abe`。
- 已生成第二阶段业务源码包：`build/business_source_artifacts/SprayBusinessDev-0.2.0.zip`。
- 业务源码包 SHA256：`362ce1cd5c341174e565f9bd989c15de196574711eb819ccccbb1a4ea0929c0f`。
- zip 根目录下 `SMRobotCore` / `SMRobotPlatform` / `thirdparty` / `data` / `build` 条目均为 0。
- staging 验证使用外部 `thirdparty` / `data` 路径，`configure` 成功，`SprayCore` Release 构建成功。
- 因 CMake 生成目录在 Windows 上偶发删除异常，脚本会把旧 staging 或验证 build 移到 `.deleting-*` / `.build-deleting-*` 后继续打包；这些目录位于包根之外，不进入 zip。
