# RS2026 1.0.6 发布比较与维护分支升级指南

日期：2026-10-26。目标：Windows x64 / Visual Studio 2019 / C++17。

本文记录1.0.6源码变化与下游迁移要求。发布验收和制品以独立发布目录中的 release-manifest.json、SHA256SUMS.txt 和最终验证记录为准；第6节是下游合并要求，不是本轮已执行证据。

## 1. 比较边界与来源

| 身份 | 精确边界 | 依据 |
|---|---|---|
| 上一正式版本 | annotated tag `v1.0.5`，提交 `1a2056d4d18e1b2173f646e992a29ac84fadca93` | 2026-09-30 tag、发布 manifest、磁盘上的三份 ZIP 相互一致 |
| 1.0.6 功能源码基点 | `branch-codex-dev` / `644f36990f8038bad8919539efbbb8c4ff499ed5` | 2026-10-26 模型环境分支合并完成后的干净工作树 |
| 本轮发布增量 | 版本号、发布脚本默认值、Change Log、本文、机器可读比较清单及实际门禁发现的必要修复 | 正式源码以门禁完成后建立的 annotated tag `v1.0.6` 与 release-manifest.json 为准；候选证据和正式制品来源分开记录 |

功能净范围：`v1.0.5..644f36990f8038bad8919539efbbb8c4ff499ed5`，28 个提交、423 个文件、19,672 行增加、5,466 行删除。完整正序提交（含 full hash、日期、父提交、经源码核对的净意图）与每个文件的增删改身份见 [机器可读比较清单](RS2026-1.0.6-source-comparison.json)。发布增量须额外比较，不能把功能基点误称为最终 1.0.6 tag。

`89c03b13` 是 1.0.5 发布后补记 checksum 的提交，处于本范围内，但不是新增产品行为。阶段过程中的临时状态以最终净源码为准，不照抄提交描述判定能力。

子模块：`data` 从 `34044d253c26ebe0ef18e2a53e389ad8940e9d44` 升到 `5e397c2fd99476382996ca4ab2a561e13df8c646`；`thirdparty` 保持 `56a5287ff45211f90ec20e4ec67c957e66de4903`。data 不是可忽略的图片附件，含配套生产 shader 和模型资源。

### 1.1 正序提交清单

| # | commit | 日期 | 最终归属/净意图 |
|---:|---|---|---|
| 1 | `89c03b134498a342c8e9681d81db394e988f112c` | 2026-09-30 | 补记1.0.5制品哈希，不新增产品行为 |
| 2 | `15183fef5ba581a9794df606cf7cd0ba494479a9` | 2026-10-04 | 补齐独立测试/探针对GTest的依赖发现 |
| 3 | `581b4ede0d4277b0d61273fdc427eed9b7237df8` | 2026-10-11 | 有效构建图、共享按需provider、预编译身份与运行时检查 |
| 4 | `76d83c4ff99641aa17e49fbc053d6a03f4767e0b` | 2026-10-11 | data校正提交；最终gitlink以净比较为准 |
| 5 | `71b17aa6bb49979bec3764882ad2543b1974e68c` | 2026-10-16 | 合并两侧开发历史，不单独视为新行为 |
| 6 | `5ec84925d5a0e08349f6359268bd4d3376830ecb` | 2026-10-16 | 清理已完成阶段计划、登记测试退休复核，不删除测试 |
| 7 | `5a03fc46a8d87b3fb0ef3698ecce2b253fea8471` | 2026-10-16 | 分层回归框架和目录级catalog治理 |
| 8 | `5fa80ffd39fc5687c77bc1f4a90f378398bec3f6` | 2026-10-19 | CMake可见缓存选项命名与旧名迁移 |
| 9 | `c77deb027af934a0c7c83c36ff177956d00f228e` | 2026-10-19 | 清除LEGACY入口；后续净状态仍允许SELECTED/FULL等范围 |
| 10 | `f6d16b35a1bde95eff814a676b0b5a00e1678ffc` | 2026-10-19 | 测试包/组件勾选层级、选择报告 |
| 11 | `a710037912fb00ba22748230ab1ed816595c957c` | 2026-10-19 | 区分诊断/探针与治理测试 |
| 12 | `15c277bb1aab2714941a606ebc58e7214203f858` | 2026-10-21 | DevTools选项和BuildChecks/Governance契约 |
| 13 | `4e8232b45093d5dfedc8f6a67e7318ac9640be7c` | 2026-10-21 | 标准INSTALL按所选第一方库包安装 |
| 14 | `70f6e56b72ea37ff3c85e6b4fcf85befaddc7858` | 2026-10-21 | 恢复默认PrebuiltPackages路径 |
| 15 | `fd967d9071d1e28a2bfb695cb465ace53b87d9d4` | 2026-10-21 | 避免staging备份目录被当成源码package |
| 16 | `b0f6d2e06bd434cc25b431fda49a5ce9cd390259` | 2026-10-21 | 恢复默认PrebuiltPackages显式消费查找 |
| 17 | `4b8fad9b1ebf90563f5a08b4916b0c1bbf2d672b` | 2026-10-21 | 清理旧安装标记/备份机制，保留无标记发现 |
| 18 | `5e528a320fe4f48aa4047f95265a3c1ac9f809e6` | 2026-10-21 | URDF/Simscape/Stewart独立候选回归 |
| 19 | `8fdc3a5d693c4a9c04a93a9d35d30070ff4163b3` | 2026-10-21 | 共享模型fixture与安装消费者默认资源入口 |
| 20 | `c9c7bfb8a34e6cd30ab7a451dbbcd3fad8c6de1e` | 2026-10-23 | Ubuntu26.04/Docker和Verification工具链接入 |
| 21 | `ab64f4e3f4e65bf43e5904e3499b3946200c5198` | 2026-10-23 | Qt Creator编译器身份与反复重配修复 |
| 22 | `251c182ac9744cf0afceab6b7f9f36ccd423cdda` | 2026-10-23 | Ubuntu运行环境集成收口 |
| 23 | `3b40fb6f88f6a5e968af7dff20c91a95377479dd` | 2026-10-24 | Verification1.0.1 Linux部署、USB采集说明；以实际平台产物为准 |
| 24 | `92ed38e7335dfb9623c20e4cb4a0e94d6232c6be` | 2026-10-24 | 本机认证/指纹相关记录及SDK接入收口，不复制个人认证材料 |
| 25 | `22a6a7d60a53eb02dfde23045fcae9516f57a845` | 2026-10-25 | Blender工厂资源与环境工作的初期提交 |
| 26 | `7fed807d2e200e7acea90d1cbc27270953878ade` | 2026-10-26 | 模型环境0–5阶段、通用材质/路径/资源生命周期、候选与部署 |
| 27 | `b9ec101490c1af30c141ca49259df3963cccb807` | 2026-10-26 | 点云/体素/集合/连续碰撞组件、Runtime/规划接入与Linux安装闭包 |
| 28 | `644f36990f8038bad8919539efbbb8c4ff499ed5` | 2026-10-26 | 合并模型环境与碰撞功能；测试清单和审计记录取并集 |

## 2. AI/开发者执行约定

本节面向**发布后在其他维护分支执行升级的人或 AI**。先读其工作区的 `AGENTS.md`，保留业务改动；记录业务 HEAD、工作区 diff、当前 SDK/data/toolchain 身份。推荐完整合并版本范围；若必须按子系统迁移，按第 5 节原子组操作。禁止以某一个 `.cpp` 已合入就宣布该功能合并完成。

完成标准：所选组的生产源码、公开头、CMake、配置、资源、回归契约/fixture/catalog 都已迁移；源码/预编译组合一致；双配置下游验证有日志，空测试列表不得算通过。测试失败先判断实现遗漏、预期变化、环境前提；不得删除断言、关闭候选集、关闭授权或改 ABI baseline 来获得绿灯。

若只拿到 Business Source：Core/Platform/Common 通过新版 PrebuiltPackages 消费，不能要求其中存在生产者私有实现或完整 regression 源码。Core/Platform 的修改和对应生产者回归在本源仓/SDK 维护分支完成；业务分支必须完成独立 consumer 和自身已有集成回归。缺少生产者测试材料时记录未覆盖项，向 SDK owner 获取对应版本证据，不能伪造完整覆盖。

## 3. 按 owner 解释最终行为

### 3.1 Collision：集合、体素与连续路径

新增 `CollisionGeometrySet.h`、`CollisionGeometrySetBuilder.h`、`CollisionGeometrySetQuery.h`、`CollisionMotionQuery.h`、`RobotMotionQuery.h`，安装公开头 inventory 从 26 个升为 31 个。

- `CollisionGeometrySetBuilder::{sampleSpheres,occupiedVoxels,voxelizedPoints}` 显式构造球样本/占据盒集合，带 element key、source indices 和 source/revision。不是从点云自动恢复封闭实体表面。体素索引表示 `origin + [i,i+1]*cellSize`；膨胀只执行一次；重复/非有限输入和容量限制有明确结果。
- `CollisionGeometrySetQuery` 对集合执行碰撞、距离、接触、间隙及批量/过滤查询；结果区分输入错误、截断和质量，TriangleMesh 仍按表面语义。不要把近似接触当作任意 mesh 的精确穿透深度。
- `CollisionMotionQuery::{path,rigid}` 接收固定几何快照与路径描述；保守模式必须有覆盖全区间的运动上界，预算不足或信息不全返回 `Indeterminate`。`NoCollisionSampled` 仅表示有限采样未命中，不能映射为安全。
- `RobotMotionQuery::jointLinear` 使用真实 `q(u)=from+u*(to-from)` 和隔离 FK，参数按完整模型 DOF 顺序，旋转为弧度、平移为米；连续角不自动取最短路。不允许用 link 端点弦代替关节轨迹圆弧。无碰撞几何的 link 需要显式声明，不完整覆盖不得返回安全。
- `CollisionScene::motionSnapshot` 冻结当前有效对象/配对，保留 detector、scope、mask、ACM 等过滤；不新增 Collision→RobotInstance 依赖，FCL 仍为私有实现。

旧 `PointCloudVoxelGrid` 容量/构建优化与新集合输入配套；不要丢掉它而只复制新头。局限包括 O(N*M) 候选组合、宽松包围盒造成不确定、非所有闭链/任意 backend 组合都可证明安全。

### 3.2 SimulationRuntime 与 ProjectMotionPlanning

`ProjectCollisionRuntime::queryRobotJointMotion` 以项目 runtime 当前过滤/机器人/挂载工具建立快照，调用公共 FK 和 Collision 服务，不修改 live joints、已有 detector result 或当前构型。

`ProjectPlanningSceneSnapshot::useConservativeMotionValidation` 显式启用保守 edge 验证；只接受 `Success + Clear`。`useSampledMotionValidation` 恢复旧模式，默认行为仍是旧采样。零预算、未知机器人、覆盖不全/FK 失败不得变成 valid。

RobotSDK facade 没有新增这些 API；第三方应按组件头/target 消费，不能假设 facade 已自动转发。

### 3.3 RobotViewerCore / AssetCore / RenderCore / RobotQtViewer

- presentation 新增 `ProjectSceneEnvironmentPartRole`、`ProjectSceneEnvironmentPart`、`ProjectSceneEnvironmentSelection`、`validateProjectSceneEnvironment`，并通过 ProjectScene/RobotViewport 选择、初始化及项目重建路径重放。原五种程序化环境和旧 ID 保留。
- 私有模型环境 helper 事务式构建完整 root；任一模型/贴图失败保留旧环境，真实 GL 初始化后才发 committed。环境不成为项目实体、选择对象或碰撞体，不改变 dirty/undo/project bounds。
- Environment 单独 pass 在 grid/shadow 前绘制；主视图的屋顶/朝向相机墙面剖切不修改共享 node visible，挂载相机始终看到完整环境。不能只改 MainWindow 菜单而遗漏 core 收集/渲染路径。
- AssetCore 修复 glTF `%20` 纹理 URI 与 UTF-8 外部 bin；RenderCore 修复中文 PNG 字节读入，Normal 真正进入 GPU，末尾新增 `MetallicRoughness` slot，G/B 分别读取 roughness/metallic 并乘 factor。旧独立灰度仍读 R。生产 shader 在 data 内，必须同步。
- TextureManager 缓存从 shared_ptr 改 weak_ptr，材质仍持有活跃纹理，最后一个使用者释放后可卸载。其模板导出会影响历史自动导出 ABI，不能据私有成员名字就认定二进制无风险。
- Qt EnvironmentController 拥有菜单、QSettings 偏好迁移、严格 JSON 适配、取消/失败/成功操作终态；MainWindow 只装配。中英文 catalog、profile 参数、精确 runtime 部署与 controller 配套。
- 内置 Blender 工厂为 7 部件、50,192 三角形、39 mesh 提交、runtime 2,909,199 bytes。运行不依赖 Blender；`.blend` 是创作源，不部署到 Runtime。

首版模型约束是不透明 glTF Separate（外部 `.bin` 和图像）；不承诺通用 GLB 内嵌图或透明玻璃。详细入口见 [模型环境说明](MODEL_ENVIRONMENTS.md)。

### 3.4 BuildSystem / package / regression 基础设施

有效源码构建图驱动按需 provider，未选择的 source-only 依赖不再强制加载。八种历史机器配置收敛到共享 provider；当前实际机器选择以 UserConfig/缓存为准，两个 3090 独立配置文件已被删除，迁移前须检查下游脚本是否硬编码旧名字。

预编译消费检查版本、平台/工具链、配置、Config 来源和二进制/private runtime；Common/各包的 target/component 开关命名统一，旧缓存名保留隐藏兼容别名。不能只迁移 GUI 选项而遗漏 resolver、metadata 和 target declarations。

普通 `INSTALL` 现在按 `InstallPrebuilt_<Package>` 安装，默认 Common/Core/Platform 到源码 `PrebuiltPackages`，事务 staging、验证后更新。正式发行仍用 `InstallSmRobotSdkProfile.cmake` 的具名 profile，不能用 full install 替代。旧 `PrebuiltPackages.previous-*` 等目录不应被误扫描为生产包。

测试由 `RS2026TestCatalog.cmake` 与 `RS2026TestGovernance.cmake` 控制；新增独立 ai/candidate 目录，旧测试保持 unknown/unreviewed，不因通过就晋升。`DevTools_*` 生成诊断程序但不是自动回归。FULL 不受局部 package 勾选树缩小，IMPACTED 按 owner 反向消费者闭包选择。

RobotIO 三组新回归固定 URDF、普通 Simscape、Stewart 的导入字段/拓扑/资源解析；共享 fixture 归 `tests/fixtures/models`，不是完整仿真/碰撞正确性的证明。源 XML、fixture 最小 mesh 与契约应整体迁移。

Linux 增加私有 runtime 闭包、SONAME/相对 RUNPATH 安装，Ubuntu 26.04 Docker/Qt Creator 的构建/运行收集入口及 Verification 1.0.1 provider。此 Windows 发布不把 Ubuntu 历史验证视作当前双平台门禁；thirdparty gitlink 未变，不得从主仓说明推导 Windows 私有 SDK 已被本轮重新编译。

### 3.5 本轮发布增量与门禁修复

- 本轮发布脚本、根版本与 Ubuntu 脚本默认值推进到1.0.6；Windows 使用命名隔离目录，不污染旧版产物。Linux 脚本仅版本同步，未实测。
- RenderCore、SceneCore、RobotRenderBridge 的实际 object 编译 target 显式使用旧版 `/Zc:inline`，防止 CMake/MSVC 生成器默认值变化把大量 COMDAT 模板暴露为 ABI；无新增依赖或 public API 改名。
- 恢复历史7个 QuickStart 的 standalone CTest 注册；主工程仍不注册 catalog 外测试。安装 matrix 使用 `BUILD_TESTING=ON`，所有 CTest 都加 `--no-tests=error`，空列表不能冒充通过。
- Business Source 打包按实际分发源码投影 catalog：保留19目录及原 owner/status/origin，排除的30个 producer/test-infrastructure目录逐项写入 `cmake/RS2026TestCatalogOmissions.json`；主仓 catalog 完整保留，校验不放宽。包外 owner 证据仍需单独提供。
- RobotQtViewer 环境资源的构建后复制与安装统一使用 `SMROBOT_DATA_ROOT`，支持 Business Source 的外部 data；config 仍属于应用源码。此项与包内 config/环境资产一起迁移。
- RenderCore 的 installed target 声明同配置 `RenderCoreShaderResources` 私有 DLL，随现有 runtime helper 部署；不增加公共链接要求。Qt 中央部署补齐已部署 `qpdf[d]` 插件所需 `Qt5Pdf[d].dll`，仍使用现有 Qt provider。
- Runtime 排除 data 旧 `export.log` 与 `shader_gen2/cpp_source` 示例源码，保留 Wavefront `.obj` 网格。
- Runtime 排除 Blender 创作源与构建/启动日志，先完成启动检查再生成文件清单；sourceDirty 明确记录候选来源。发布 manifest 用子模块提交与归档名记录来源，避免写入开发机根路径。
- 2026-10-26 用户已批准“所有 SDK/业务消费者重新编译”的 ABI 边界。v7不改，新增v8；删除154/新增270的分类、工具默认值排查与批准见 [ABI评审](RS2026-1.0.6-abi-review.md) 和 [逐项清单](RS2026-1.0.6-abi-diff.json)。这不是相对1.0.5二进制兼容声明。

## 4. API、格式与配置迁移

| 边界 | 1.0.5 | 1.0.6 | 迁移动作 |
|---|---|---|---|
| 项目文件 | `.sys.json` version 3 | 本范围未改 schema | 旧项目继续读取；新视觉背景不写入项目 |
| Product Profile / plugin | v2 / v1 | schema 保持 | 不做无依据版本重写 |
| 环境描述 | 无模型描述文件 | `smrobot.presentation-environment` version 1 | 新增独立配置，保留源 glTF/bin/image 目录关系 |
| 环境偏好 | 旧程序化 preset | 旧 preset + source/模型/剖切偏好 | 通过 controller 迁移；失败不覆盖偏好，不手改项目 |
| 测试启用 | `BUILD_TESTING` 等分散入口 | `Test_Enabled`、`Test_Scope`、三个治理开关 | 新构建树显式开启 candidate，默认 OFF 会漏掉本次新增保护 |
| 局部测试 | 旧 regression/LEGACY | SELECTED/COMPONENT/PACKAGE/IMPACTED/FULL | LEGACY 缓存迁为 FULL；发布后整版跨 owner 合并用 FULL |
| 手动探针 | TestDiagnostics 等 | `DevTools_Diagnostics/FeatureProbes/PackageConsumers` | 旧名兼容，不能计作 CTest |
| 普通 INSTALL | 宽范围安装 | 选中第一方库包 | 检查 CMAKE_INSTALL_PREFIX，正式 SDK 用具名 profile |
| package 消费 | 旧名称/宽松查找 | 新显式 target/component + 身份/runtime 检查 | 不混用旧 import lib、新 DLL、旧 public headers |
| DLL/静态 ABI | v7 baseline / 旧 viewport 布局 | 本轮新增入口及 RenderCore cache 导出变化 | 独立比较并评审；静态 ViewerCore/RobotViewport 必须重编，不宣称直接替换二进制 |

新环境最小格式（额外字段和完整墙面见仓库 config）：

```json
{"schema":"smrobot.presentation-environment","version":1,"units":"meters","scale":1,
 "parts":[{"id":"floor","role":"floor","source":"runtime/floor.gltf"}]}
```

不把模型环境写成旧 `.sys.json` 的新实体。`normal` 为墙单位外法线，offset 满足 `dot(position,normal)=offset`；按米传入，不沿用机器人 bounds 的自动缩放。

碰撞结果调用示意（先检查 status，再检查 verdict）：

```cpp
const bool safe = result.status == collision::CollisionStatus::Success &&
                  result.verdict == collision::MotionVerdict::Clear;
```

不把 `NoCollisionSampled` 或 `Indeterminate` 映射成 safe；`hitParameter` 不是已认证的 earliest TOI。

## 5. 必须共同合并的原子组与冲突处理

| 顺序/组 | 必须共同迁移的边界 | 漏合并迹象 |
|---|---|---|
| G1 构建与测试基础 | 根 CMake；cmake dependency/provider/cache/prebuilt/test/install 文件；各包 PackageConfigSetting/CMake；catalog 与父级 rs2026_add_test_unit；共享 fixture | configure 要求无关依赖、测试 target 不生成、目录未登记、旧包被误识别 |
| G2 Collision | 5 新公开头 + umbrella/manifest；集合/路径/RobotMotion 实现；CollisionScene/PointCloudVoxelGrid；CollisionQueryTests/契约；安装示例/CMake | 编译有头但链接无符号、安装漏头、采样误报安全、FK 圆弧漏碰撞 |
| G3 Runtime/规划 | ProjectCollisionMotionQuery.cpp、runtime 公开头与 CMake；规划声明/实现及 ConservativeMotionTests | 源码加了却未进 target、保守模式仍走旧采样、修改 live state |
| G4 模型环境 | AssetCore/RenderCore 改动；ViewerCore DTO/helper/viewport/pass；Qt controller/source/MainWindow/main；config/翻译/部署；data gitlink/shader/资产；候选契约/CMake | 菜单出现但无背景、MR 颜色错、缺图、失败覆盖旧场景、挂载相机剖切错误、内存常驻 |
| G5 发布接入 | 版本/脚本；SDK/install runtime metadata；下游实际消费的全部新版 headers/lib/bin/Config；CHANGELOG 与本指南 | 版本混用、源码与预编译重复、缺私有 DLL、只在开发机可运行 |

G1 先于 G2/G3/G4 的 CMake/test 部分；G2 先于 G3；G4 不依赖 G3 的业务逻辑，但材质和 shader/资源不可拆散发布。维护分支应保留其已有业务 Workbench 注册/产品 profile，只迁移基础能力，不用上游整文件覆盖业务贡献。

冲突热点：根 CMake/PackageConfigSetting、MainWindow 装配、ProjectScene/RobotViewport、Runtime/规划入口、catalog 尾部、翻译 JSON、agent 快照/审计。catalog 同位置追加必须取并集，不能二选一；快照/审计保留双方记录。不要恢复已删除的旧计划作为实现入口，也不要从 license/AuthN 历史提交复制个人认证材料。

## 6. 下游合并时必须执行的 regression

本节是**其他团队合并后的执行要求**，不是要求本轮发布前额外运行 FULL。执行者记录自己平台、机器 profile、build root、SDK 和 data commit、所选 profile 与测试列表。原仓完整合并多 owner 时统一采用以下范围：

```powershell
$mergeBuild = 'build/upgrade106'
$userOption = 'USER_TANGTANG_p15v3_Windows' # 换成该维护分支真实机器 profile
cmake -S . -B $mergeBuild -G 'Visual Studio 16 2019' -A x64 `
  "-D${userOption}=ON" -DTest_Enabled=ON -DTest_Scope=FULL `
  -DTest_IncludeCurated=ON -DTest_IncludeCandidate=ON `
  -DTest_IncludeUnreviewed=ON -DTest_IncludeSlow=ON
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
foreach ($configuration in @('Debug','Release')) {
  cmake --build $mergeBuild --config $configuration --target RobotQtViewer RS2026_BuildSelectedTests Collision-PublicHeaderCompile
  if ($LASTEXITCODE -ne 0) { throw "Build failed: $configuration" }
  ctest --test-dir $mergeBuild -C $configuration -N
  if ($LASTEXITCODE -ne 0) { throw "CTest inventory failed: $configuration" }
  ctest --test-dir $mergeBuild -C $configuration --output-on-failure --no-tests=error
  if ($LASTEXITCODE -ne 0) { throw "Regression failed: $configuration" }
}
```

`RS2026TestSelection.md` 应在本完整源码 profile 中选中 49 目录：7 candidate、42 unreviewed、0 curated。目录数不是 CTest 个案数；GoogleTest 在构建/发现后展开，不硬编码历史总用例数。下游若增加了测试应解释差异；因产品 profile 未启用 owner 而缺测试，须记录缺失消费者，不能以旧数字强行通过。

只有真正单 owner 的局部迁移才用 `-DTest_Scope=IMPACTED -DTest_Package=<owner-package> -DTest_Component=<owner-component> -DTest_ImpactKind=PUBLIC_API`（build/schema/runtime 分别用对应类别）；不得用 COMPONENT 的局部绿灯代替整版合并。IMPACTED 不得加子模块过滤缩小闭包。

| 触发组 | 必须检查存在且通过的测试/构建入口 | 保护内容 |
|---|---|---|
| 所有组/G1 | SoftwareArchitecture-BoundaryCheck、RS2026TestGovernanceCatalogInventory、RS2026TestGovernanceRejectsUncatalogedDirectory、BuildGraph/Provider/PrebuiltIdentity 场景 | owner 边界、测试未漏登记/重复、source/prebuilt 与私有 runtime |
| G1/RobotIO fixture | RobotIO-ModelLoadUrdfTests、RobotIO-ModelLoadSimscapeTests、RobotIO-ModelLoadStewartTests；对应 CTest 前缀按各目录 CMake 核对 | Panda 质量/惯量/球碰撞与 glTF 路径；Simscape 单位；Stewart 拓扑/资源 |
| G2 | Collision-QueryTests（`CollisionQuery.*`）、Collision-PublicHeaderCompile、旧 Collision 回归 | 球/体素数据、双向集合距离/接触、薄板/旋转/双方运动、取消/预算、真实 FK 圆弧、工具/ACM、状态隔离 |
| G3 | ProjectPlanning-ConservativeTests（ProjectPlanningConservativeMotion）、旧 Headless 与 runtime collision query 回归 | 零预算/未知/覆盖不足不放行、live/snapshot 不变、原采样兼容 |
| G4 | RobotQtViewer-ModelEnvironmentTest（RobotQtViewerModelEnvironment）、ProjectSceneSystems、Localization、OperationStatus、已有 GUI/document/runtime 回归 | 真 GL shader 通道、七部件/39 mesh、中文路径、失败回滚、主/挂载相机隔离、50次卸载、偏好/操作终态 |
| G5 | 安装后的 QuickStart 与独立 consumer matrix、实际使用 package 的 Debug/Release 构建/运行 | 新公开头/符号和私有 DLL 完整、可移动部署、源码消费者重编 |

定向排障可先用 `ctest -R`，但最终合并验收仍使用所选完整 scope。ModelEnvironmentTest 必须有真实 OpenGL 4.2+ 与 data 资源，不能 offscreen 跳过后计成功；授权测试需要匹配该机器的合法许可证，保持 Enforce。

候选开关负例（适用于维护测试框架或 G1/G2/G4 合并）：在第二独立构建目录设置 `Test_IncludeCandidate=OFF`，确认新增 ModelEnvironment、Collision-QueryTests、ConservativeTests 不生成；打开后重新 Configure，确认 target/CTest 回来。无需删除测试源码或改变身份。

受控红绿复核只在下游怀疑保护无效时执行：在隔离 fixture 副本改变 Panda mass/Simscape units/Stewart mass，或在单独暂存 shader 将 MR metallic B 改 R，应触发对应断言；随后逐字节恢复并核对 hash，再重跑。不得在生产工作树并发构建时编辑 shader，不得保留故障注入。

### 6.1 SDK 与业务源包消费者的独立门禁

先用干净安装前缀运行具名 `core_platform_standalone` profile，再用 `RunSmRobotSdkConsumerMatrix.cmake` 对 Debug/Release 检查安装后例子；FCL/Qt/外部 dependency root 从消费方 profile 解析。ABI baseline 使用该正式版本**已批准**的 v8 baseline，不能临时采用当前 snapshot 自比自通过。

Business Source 分支缺 Core/Platform regression 源码时，必须用随包的新 PrebuiltPackages 重新配置/编译双配置的 RobotQtViewer 和业务 targets，运行自己的集成回归；保留 SDK owner 的对应发布证据。涉及自定义 GPU/Viewport 对象时必须重新编译，不能混用 1.0.5 static lib。

### 6.2 下游验收记录模板

```text
upgrade_from=<业务分支HEAD + SDK版本>
upgrade_source=<正式1.0.6 tag/提交与归档SHA256>
source_groups=G1,G2,G3,G4,G5
data_commit=<...>
sdk_identity=<version/configs/compiler/profile>
test_scope=FULL
governance=curated,candidate,unreviewed;slow=ON
configure_and_build=<Debug/Release 日志>
ctest_inventory=<实际目录/用例；差异解释>
ctest_result=<通过/失败/未执行；失败契约与原因>
installed_consumers=<独立构建与runtime日志>
abi=<正式baseline/结果；未验证不能填通过>
uncovered_consumers=<GLFW/Linux/业务硬件等>
merge_complete=<yes/no；阻塞与owner>
```

## 7. 旧制品基线与本轮发布证据

旧版三份归档已从磁盘重算并与 1.0.5 manifest 核对一致：

| 1.0.5 归档 | bytes | SHA256 |
|---|---:|---|
| RS2026-BusinessSource-1.0.5-win64-vs2019.zip | 62,912,083 | `6b5a78d20521595c2df20e3c9740ed9d87513418ec00136fd293cb1f7d68c715` |
| RS2026-Runtime-1.0.5-win64.zip | 404,612,044 | `689997e2222f38e296c6fc36d03d272ecd3ff47172f90c58f91737da5be35b95` |
| SMRobotCorePlatformPreBuild-1.0.5-vs2019-x64.zip | 92,614,425 | `0ae58b251be9fdbb10fbab2ecd1d4d38609cb221521977f74156dc158af29105` |

本轮只执行发布自身必要门禁，不额外执行本指南要求下游合并时运行的 FULL。最初启动的 FULL 配置在用户澄清后中止于 generate 前，未进入编译/CTest，不记作通过或失败。

### 7.1 源码冻结时的候选验证

- VS2019 x64，MSVC 19.29.30158，CMake4.4.0，C++17，Enforce；SDK/Runtime候选使用p15v3，Business Source消费者使用随包保留的4090 profile；依赖默认根D:/PreBuild及外部FCL由profile解析。
- SDK `build/r106s` 双配置生产targets构建、`core_platform_standalone` 安装及layout通过；`Test_Enabled=OFF`。独立consumer22项 + 6个安装后QuickStart合计8项，每配置30/30通过；这是SDK独立消费检查，不属于主仓治理集合。
- v7差异已分类并经用户批准；候选v8 snapshot一致。SimulationRuntime/SensorSimulation严格审计4个section、296符号、0发现；自动导出inventory仍有395发现（12 section、13031符号），作为历史迁移债务记录，不称严格通过。
- Business Source第一次configure发现完整catalog引用未分发producer目录；第二次Debug编译完成后发现环境部署绕过外部data根。修复catalog投影与资源入口后，独立`build/r106bv`的Debug/Release RobotQtViewer均构建成功；未改变断言或删生产者测试。打包锁文件现对包内实际manifest计算hash，去除PDB后同步policy。
- Runtime候选在Enforce模式正常退出；使用包内config/data加载420-red4600项目与factory-detailed环境成功，真实OpenGL截图889x669，确认7部件模型可渲染。日志有既有QString::arg占位警告，未影响退出或渲染；不据截图证明全部材质、相机或collision回归。
- 上述为dirty候选证据。接下来从干净最终源码提交重建正式SDK/Viewer并打包；正式归档大小、SHA256、新旧逐文件差异与来源由发布后验收记录补记。不得把候选包称为最终可复现制品。

## 8. 来源限制与排除项

本次没有新增第三方依赖或改 C++ 标准。Ubuntu 26.04/22.04/24.04、异机 GPU、真实硬件和完整 GLFW 人机工作流不属于本轮 Windows 发布的实测环境。data 中 Blender 创作源不嵌入 Runtime；Business Source 不重复携带 Core/Platform 源码和整套 data。

RenderCore 历史自动导出模板变化已经分类；用户已批准本版要求消费者重新编译，新增 v8 并保留 v7。不承诺旧二进制直接替换；源码消费者成功与二进制兼容是独立结论。
