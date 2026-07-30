# SMRobot SDK thin / standalone 使用说明

本文说明当前仓库中两类 Core/Platform SDK profile 的用途和日常使用方法。

## 适用对象

本文面向两类人员：

1. Core/Platform 维护者：负责从完整源码仓库构建 SDK。
2. 业务开发包维护者：负责生成去掉 `SMRobotCore` / `SMRobotPlatform` 源码的业务源码包。

业务开发成员通常不需要重新构建 SDK，只需要拿到业务源码包、thin SDK、thirdparty 和 data。

## 两种 SDK profile

当前 `scripts/build_sdk.ps1` 支持两种推荐 profile：

| profile | 用途 | 是否包含 thirdparty | 是否包含 data | 推荐场景 |
| --- | --- | --- | --- | --- |
| `core_platform_thin_internal` | 内部开发 SDK | 否 | 否 | 内部团队高频开发 |
| `core_platform_standalone` | 完整离线 SDK | 是，按安装组件复制 | 否 | 离线客户、外部交付、没有统一依赖环境的机器 |

旧 profile `core_platform_dll_e_final` 仍然保留，主要用于兼容历史命令。新的默认值是 `core_platform_thin_internal`。

## 推荐目录关系

内部开发推荐把 SDK、thirdparty、data 分开管理：

```text
D:\sdk\
  SMRobotSDK-1.4.0-thin\
  SMRobotThirdParty-vs2019-x64-2026.07\
  SMRobotData-2026.07\
```

在本机快速验证时，也可以先使用仓库内已有目录：

```text
D:\program\src\RS2026_CodexDev\build\sdk_thin_internal
D:\program\src\RS2026_CodexDev\thirdparty
D:\program\src\RS2026_CodexDev\data
```

## 构建 thin SDK

内部团队默认构建 thin SDK：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 `
  -Profile core_platform_thin_internal `
  -SdkPrefix build\sdk_thin_internal `
  -AllowDirtyTree `
  -CleanInstall
```

因为 `scripts/build_sdk.ps1` 的默认 profile 已经是 `core_platform_thin_internal`，所以下面命令等价：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 `
  -SdkPrefix build\sdk_thin_internal `
  -AllowDirtyTree `
  -CleanInstall
```

生成结果应类似：

```text
build\sdk_thin_internal\
  cmake\
  Common\
  SMRobotCore\
  SMRobotPlatform\
  SMRobotSDKManifest.json
  SMRobotSDKExportSnapshot
```

thin SDK 根目录不应包含：

```text
thirdparty\
data\
build\
```

## 构建 standalone SDK

如果要给离线客户或没有统一 thirdparty 环境的机器，使用 standalone profile：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_sdk.ps1 `
  -Profile core_platform_standalone `
  -SdkPrefix build\sdk_standalone `
  -AllowDirtyTree `
  -CleanInstall
```

standalone SDK 会比 thin SDK 大。它的目标是尽量降低外部机器的依赖准备成本。

## 生成业务源码包

业务源码包用于给喷涂、打磨、移动搬运等业务团队继续开发。它会删除 staging 中的：

```text
SMRobotCore\
SMRobotPlatform\
```

第二阶段默认也不复制：

```text
thirdparty\
data\
build\
```

推荐命令：

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

如果希望生成时顺手验证业务包能 configure/build：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.2.0 `
  -SdkPrefix build\sdk_thin_internal `
  -ThirdPartyRootHint D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07 `
  -DataRootHint D:\sdk\SMRobotData-2026.07 `
  -AllowDirtyTree `
  -CleanStaging `
  -VerifyBuild `
  -VerifyBuildTarget SprayCore
```

生成结果：

```text
build\business_source_staging\SprayBusinessDev-0.2.0\
build\business_source_artifacts\SprayBusinessDev-0.2.0.zip
```

## 下游业务团队如何使用

业务团队拿到业务源码包后，进入包根目录：

```powershell
cd SprayBusinessDev-0.2.0
```

如果 `sdk.lock.json` 中已经写入了正确的外部依赖路径，可以直接运行：

```powershell
powershell -ExecutionPolicy Bypass -File configure_business_prebuilt.ps1 `
  -Build `
  -BuildTarget SprayCore
```

如果依赖路径在下游机器上不同，运行时显式指定：

```powershell
powershell -ExecutionPolicy Bypass -File configure_business_prebuilt.ps1 `
  -ThirdPartyRoot D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07 `
  -DataRoot D:\sdk\SMRobotData-2026.07 `
  -Build `
  -BuildTarget SprayCore
```

如果只想 configure，不构建：

```powershell
powershell -ExecutionPolicy Bypass -File configure_business_prebuilt.ps1 `
  -ThirdPartyRoot D:\sdk\SMRobotThirdParty-vs2019-x64-2026.07 `
  -DataRoot D:\sdk\SMRobotData-2026.07
```

## sdk.lock.json 的作用

业务源码包根目录会生成：

```text
sdk.lock.json
```

它记录：

- 业务包名称和版本；
- 来源 commit；
- SDK prefix 的 manifest / export snapshot hash；
- thirdparty 的路径提示和 git commit；
- data 的路径提示和 git commit；
- CMake 需要启用的 `UsingPrebuilt_*` 列表。

它不是 CMake 必须读取的文件，而是给人、脚本和 AI 看的发布锁定文件。

## 常见选择

内部团队日常开发：

```text
thin SDK + 外部 thirdparty + 外部 data + 业务源码包
```

外部客户或离线交付：

```text
standalone SDK + 业务源码包
```

临时完整业务包：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_business_source.ps1 `
  -PackageName SprayBusinessDev `
  -BusinessVersion 0.2.0 `
  -SdkPrefix build\sdk_thin_internal `
  -IncludeThirdParty `
  -IncludeData `
  -AllowDirtyTree `
  -CleanStaging
```

## 注意事项

- `-AllowDirtyTree` 只建议本地验证使用；正式发布前应从干净 commit 生成。
- `-CleanInstall` 会清理 SDK install prefix。
- `-CleanStaging` 会替换同名业务包 staging 目录。
- Windows 上 CMake 生成目录偶尔删除失败时，脚本会把旧目录移动为 `.deleting-*` 或 `.build-deleting-*`，这些目录不进入 zip，可后续手动清理。
- 如果业务包 configure 时报找不到 `tinyxml2`、`Eigen`、`glm`、`nlohmann_json`，优先检查 `-ThirdPartyRoot` 是否指向正确的 thirdparty 根目录。
- 如果运行时报找不到模型、shader、点云或项目数据，优先检查 `-DataRoot` 是否指向正确的 data 根目录。
