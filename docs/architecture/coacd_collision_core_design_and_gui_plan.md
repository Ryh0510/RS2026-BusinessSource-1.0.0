# COACD 碰撞简化核心设计与 GUI 接入计划

## 1. 背景和当前结论

本轮验证已经证明，COACD 本身可以在 `SMRobotCore::Collision` 层被稳定封装为“输入三角网格，输出若干凸分解三角网格”的函数。成功案例包括：

- `SMRobotCore/Collision/include/Collision/CollisionCoacdDecomposition.h`
- `SMRobotCore/Collision/src/CollisionCoacdDecomposition.cpp`
- `SMRobotCore/Collision/regression/CoacdDecompositionTests`
- `SMRobotCore/Collision/feature_probes/CoacdDecompositionViewer`

这个方向是正确的：COACD 是碰撞几何生成能力，不应该由 `RobotQtViewer` 持有算法语义。GUI 后续应只负责发起请求、显示结果、把生成结果写回项目文档或临时预览状态。

## 2. COACD 是否有参数

有。当前封装暴露在 `CollisionCoacdOptions` 中，位于：

`SMRobotCore/Collision/include/Collision/CollisionCoacdDecomposition.h`

当前参数如下：

| 参数 | 当前默认值 | 含义和影响 |
| --- | --- | --- |
| `threshold` | `0.05` | 分解误差阈值。通常越大越粗，生成块可能更少；越小越精细，耗时增加。 |
| `maxConvexHull` | `-1` | 最大凸包数量。`-1` 表示不显式限制；设置为 8、16、32 可控制粗分解程度。 |
| `preprocess` | `"auto"` | 预处理模式，当前映射为 `auto/on/off`。非闭合、非流形 STL 通常需要预处理。 |
| `prepResolution` | `50` | 预处理分辨率。越高越慢，可能更细。 |
| `sampleResolution` | `2000` | 采样分辨率。越高越慢，结果更稳定。大模型建议 GUI 默认降低，例如 1000。 |
| `mctsNodes` | `20` | MCTS 搜索节点数。增加会提升搜索质量但变慢。 |
| `mctsIteration` | `150` | MCTS 迭代次数。增加会提升质量但变慢。 |
| `mctsMaxDepth` | `3` | MCTS 最大深度。 |
| `pca` | `false` | 是否启用 PCA 对齐。 |
| `merge` | `true` | 是否合并凸包。通常建议开启。 |
| `decimate` | `false` | 是否简化输入/输出网格。 |
| `maxChVertex` | `256` | 单个凸包最大顶点数。 |
| `extrude` | `false` | 是否挤出。 |
| `extrudeMargin` | `0.01` | 挤出边距。 |
| `approximationMode` | `"ch"` | 近似模式，当前支持 `"ch"` 和 `"box"` 映射。 |
| `seed` | `0` | 随机种子。用于可重复性。 |
| `realMetric` | `false` | COACD 底层 real metric 选项。 |

建议 GUI 第一版只暴露少数安全参数：

- `threshold`
- `maxConvexHull`
- `sampleResolution`
- `mctsIteration`

其余参数先放到高级设置或暂不暴露。原因是用户需要控制“粗到什么程度”和“计算多久”，而不需要一开始面对全部 COACD 内部搜索参数。

## 3. 这个函数是否可以成为独立库

可以，而且当前已经基本具备库接口形态：

```cpp
collision::CollisionCoacdResult result =
    collision::CollisionCoacdDecomposition::decompose(inputMesh, options);
```

当前接口优点：

- Public header 不包含 `CoACD/coacd.h`。
- 输入输出只使用 `CollisionCoacdMesh`、`CollisionCoacdOptions`、`CollisionCoacdResult`。
- 调用方不需要知道 COACD 的 C API、内存释放方式、mesh array 格式。
- `isAvailable()` 可以让 GUI 在 COACD 不可用时禁用按钮或显示明确提示。

当前接口不足：

- `CollisionCoacdMesh` 只是裸三角网格，未表达坐标系、单位、源文件、局部 transform、mesh role。
- `CollisionCoacdResult` 只有 `parts/error/elapsedMs`，没有 warnings、输入统计、输出统计、参数回显。
- `decompose()` 是同步调用，大模型可能耗时 50-100 秒；GUI 需要放到后台任务，不能在主线程直接调用。
- 当前接口只完成“几何分解”，没有负责“写 OBJ/STL 文件”“生成项目 override 元素”“选择 active collision model”。这些不应进入最底层算法函数。

结论：`CollisionCoacdDecomposition` 可以作为独立调用的核心算法接口，但 GUI 接入前建议在 `SMRobotCore::Collision` 内再补一层更面向业务的 `CollisionCoacdService` 或 `CollisionCoacdAssetBuilder`，用于统一统计、默认参数和结果校验。是否写文件则需要谨慎分层，见第 7 节计划。

## 4. 动态库、静态库和 COACD 依赖关系

当前 `SMRobotCore/Collision/CMakeLists.txt` 创建的是：

```cmake
add_library( Collision SHARED ... )
add_library( SMRobotCore::Collision ALIAS Collision )
```

所以目前 `Collision` 是一个动态库，即 `Collision_shared_rx64.dll`。

当前 COACD 的接入方式是：

```cmake
target_link_libraries(Collision PRIVATE CoACD::coacd)
target_compile_definitions(Collision PRIVATE COLLISION_HAS_COACD=1)
```

`thirdparty/Windows/FindCoACD_3rdParty.cmake` 里明确说明：

- `CoACD::coacd` 是 shared library target。
- `CoACD::_coacd` 是 upstream shared library target。
- 同时存在 `CoACD_STATIC_LIB_RELEASE`，但当前没有使用它。

因此当前依赖关系是：

```text
调用方
  -> SMRobotCore::Collision / Collision_shared_rx64.dll
      -> CoACD runtime dll
      -> tbb12.dll 等运行时依赖
```

这意味着：

- 外部调用方不需要 include `CoACD/coacd.h`。
- 外部调用方不需要直接调用 COACD API。
- 外部调用方链接 `SMRobotCore::Collision` 即可获得 COACD 封装接口。
- 运行时仍然需要能找到 CoACD 的 DLL 和 TBB DLL。
- 当前不是“把 COACD 静态打进 Collision DLL”的模式。

如果未来要完全隔离运行时 DLL，可以评估改为链接 `CoACD_STATIC_LIB_RELEASE`。但这不是简单替换，需要确认：

- COACD 静态库是否完整包含所有实现。
- 是否仍然依赖 TBB 动态库或其他运行时。
- Release/Debug 配置是否完整。
- COACD license 和再分发方式是否允许静态打包。
- CMake imported target 是否需要单独构造 `CoACD::coacd_static`。

当前推荐：先保持 `Collision` 动态库依赖 `CoACD::coacd`，但在包安装和运行目录复制中保证 CoACD/TBB DLL 被随 `Collision` 一起部署。这样可以实现“源码和 public API 层面的隔离”，不急于做“二进制静态封装”。

## 5. 这个动态库应该写在什么位置

推荐位置：继续放在 `SMRobotCore::Collision` 组件内。

理由：

- COACD 的输入输出是碰撞三角网格，不依赖 Qt、OpenGL、SceneCore 或项目文档。
- `Collision` 组件已经包含 `CollisionShapeDesc`、`CollisionGeometryRole`、`RobotCollisionModel`、`CollisionProxyGenerator` 等碰撞几何能力。
- GUI、SDK、测试、GLFW example 都可以调用 `SMRobotCore::Collision`，不会被 Qt 绑定。
- 当前 `Collision` 本身已经是 `SMRobotCore` 包里的 component。包外使用方式应是 `find_package(SMRobotCore COMPONENTS Collision)`。

不建议把算法接口放在：

- `RobotQtViewer`：会把核心算法绑定到 Qt。
- `RobotViewerCore`：比 Qt 好一些，但仍属于 Apps 层，并且依赖 `AssetCore`、`SimulationProject`、运行时对象、项目资产路径。
- `SMRobotPlatform`：除非做“从项目文档生成资产”的平台服务，否则纯 COACD 分解不需要 Platform。

如果将来需要更细 component，可以考虑新增 `SMRobotCore::CollisionCoacd` 或 `SMRobotCore::CollisionProcessing`。但当前没有必要，因为 `Collision` 已经是独立动态库，且接口没有污染 public COACD 依赖。

## 6. GUI 调用接口是否合理、是否够用

### 6.1 当前算法接口够用的部分

对于最小 GUI 功能，即“用户选择一个 mesh，计算 COACD，显示结果”，当前接口已经够用：

```cpp
collision::CollisionCoacdMesh input;
collision::CollisionCoacdOptions options;
collision::CollisionCoacdResult result =
    collision::CollisionCoacdDecomposition::decompose(input, options);
```

GUI 只需要准备三角 mesh，然后把 `result.parts` 转成显示 mesh 即可。

### 6.2 当前接口不够用的部分

对于正式项目集成，即“把结果保存为项目碰撞模型并可选择/保存/重新加载”，当前接口不够用。缺少以下业务层结构：

- 输入 mesh 来源描述：visual、existing collision、object visual、tool attachment visual。
- 输出资产格式：OBJ/STL/内部 mesh。
- 输出路径策略：`appGenerated://collision/coacd/...`。
- 项目 override 元素生成：robot link element、object collision element。
- active model selection 更新。
- 文档 dirty 状态和刷新通知。
- 任务进度、取消、错误详情。

这些不应该塞进最底层 `CollisionCoacdDecomposition`。推荐分层：

```text
SMRobotCore::Collision
  CollisionCoacdDecomposition
    只负责 mesh -> convex mesh parts

SMRobotPlatform::SimulationProject 或 RobotViewerCore 过渡层
  从 RuntimeRobot/RuntimeSceneObject/ProjectDocument 提取 mesh
  调用 CollisionCoacdDecomposition
  写 appGenerated 资产
  生成 CollisionElementOverrideDesc

RobotQtViewer / RobotQtModules
  发起命令
  后台执行
  应用 ProjectDocumentService mutation
  通知 viewport/tree/panel 刷新
```

## 7. 原 RobotQtViewer/RobotViewerCore 实现为什么容易失败

扫描结果显示，旧实现的入口在：

- `SMRobotApps/RobotQtViewer/RobotQtViewerViewportServicesAdapter.cpp`
- `SMRobotApps/RobotViewerCore/RobotViewport.cpp`
- `SMRobotApps/RobotViewerCore/ProjectScene.cpp`
- `SMRobotApps/RobotViewerCore/RobotCollisionProxyGenerator.cpp`

实际 COACD 生成主要在 `RobotCollisionProxyGenerator.cpp`。

旧实现不是完全错误，它已经调用了新的 `CollisionCoacdDecomposition`。但它的问题是职责太集中：

1. `RobotCollisionProxyGenerator` 同时做 mesh 提取、COACD 调用、hash、OBJ 写文件、manifest 写文件、`appGenerated://` 路径拼装、project override element 拼装。
2. COACD 参数在 `generateCoacdMeshes()` 内部直接默认构造，GUI 无法显式传入参数。
3. 失败返回基本是 `false`，错误信息丢失，GUI 很难知道是 mesh 为空、COACD 不可用、分解失败、写文件失败，还是项目路径失败。
4. 对工具附件有特殊临时对象逻辑，依赖运行时对象和 visualPath，输入 mesh 来源不稳定。
5. 生成结果写成 OBJ 后依赖项目资产解析、override 应用、active model selection 和 viewport 刷新；任何一环没有刷新，用户看到的就是“没有显示”。
6. 这条链路属于 Apps/Viewer 层，难以用 headless regression 直接验证完整行为。

所以之前失败更像是集成链路失败，而不是 COACD 算法函数本身失败。当前 GLFW 案例成功，是因为它把问题缩小为：

```text
STL -> CollisionCoacdMesh -> CollisionCoacdDecomposition -> OpenGL display
```

没有项目文档、资产写入、override 选择和 Qt 刷新链路。

## 8. 原来 RobotQtViewer 那部分可不可以删除

不能立刻整体删除，但应该迁移后删除重复算法逻辑。

建议分类：

### 可以迁移并最终删除的部分

- `RobotCollisionProxyGenerator.cpp` 中直接调用 COACD、写 OBJ、生成 manifest 的私有散装逻辑。
- COACD 默认参数硬编码。
- 只返回 `false` 的错误吞掉逻辑。
- 与 COACD mesh hash/part 文件命名相关的重复实现。

迁移后应由新的 Core/Platform 服务统一实现。

### 暂时保留的部分

- 从 `RuntimeRobot`、`robot::RobotLink`、`RuntimeSceneObject`、tool attachment 提取输入 mesh 的逻辑。它依赖 `AssetCore`、运行时对象和项目语义，不属于 `SMRobotCore::Collision`。
- `CollisionElementOverrideDesc`、`ObjectCollisionElementOverrideDesc` 的生成。它属于 `SimulationProject` 文档语义，不属于纯 Collision。
- Qt adapter 和 viewport service 接口。它们可以保留为调用新服务的薄壳。

### 删除条件

在以下条件满足后，可以删除旧 COACD 生成路径：

1. 新服务支持 visual mesh、existing collision mesh、object/tool attachment mesh 三类输入。
2. 新服务返回结构化错误信息。
3. 新服务可写 `appGenerated://collision/coacd/...` 资产，或提供清晰的文件输出回调。
4. GUI 已改为调用新服务。
5. 回归测试覆盖 `420-tool.STL` 或等价大模型。
6. 项目文档保存/加载后能重新显示生成的 COACD 模型。

我的判断：把 COACD 核心集中到 `SMRobotCore::Collision` 是更合理的方向；但把项目资产写入和 Qt 刷新也全部塞进 `Collision` 则不合理。正确拆法是“核心算法进 Collision，项目生成服务进 Platform/ViewerCore 过渡层，Qt 只发命令”。

## 9. 下一步修改计划

### 阶段 1：强化 `SMRobotCore::Collision` 的 COACD 核心接口

目标：让 GUI 和测试调用同一个稳定核心。

修改建议：

1. 保留 `CollisionCoacdDecomposition` 作为底层函数。
2. 扩展 `CollisionCoacdResult`：
   - `inputVertexCount`
   - `inputTriangleCount`
   - `outputPartCount`
   - `outputTriangleCount`
   - `warnings`
3. 增加 `CollisionCoacdOptions::fastPreviewDefaults()` 或独立 helper，给 GUI 一个快而可控的默认参数。
4. 增加 headless regression，固定测试 `data/Spray420/420-tool.STL`。

验收：

- `Collision-CoacdDecompositionTests` 通过。
- `Collision-CoacdDecompositionViewer --compute-only` 通过。

### 阶段 2：抽出“COACD 资产生成服务”

目标：替代 `RobotCollisionProxyGenerator.cpp` 内部散装的 COACD 写文件逻辑。

候选位置：

- 首选：`SMRobotPlatform::SimulationProject`，因为它知道 `appGenerated://`、项目资产根目录和 document override。
- 备选：暂留 `RobotViewerCore`，但实现变成薄封装，内部调用新的 Platform 服务。

服务接口建议：

```cpp
struct CollisionCoacdAssetBuildRequest
{
    collision::CollisionCoacdMesh inputMesh;
    collision::CollisionCoacdOptions options;
    std::filesystem::path outputRoot;
    std::string sourceKey;
    std::string targetKey;
    std::string source;
    std::string role = "CoACD";
};

struct CollisionCoacdAssetBuildResult
{
    bool ok = false;
    std::string error;
    std::string hash;
    std::vector<std::filesystem::path> writtenFiles;
    std::vector<std::string> appGeneratedMeshPaths;
    collision::CollisionCoacdResult decomposition;
};
```

验收：

- 不启动 Qt，也能生成 OBJ/manifest。
- 输出路径和 `appGenerated://collision/coacd/...` 一致。
- 错误信息可定位失败环节。

### 阶段 3：迁移 GUI 调用链

目标：让 `RobotQtViewer` 调用新的服务，而不是继续使用旧散装实现。

修改建议：

1. 保留 `RobotQtViewerViewportServicesAdapter` 作为 UI adapter。
2. `ProjectScene` 仍负责从运行时和项目文档定位 robot/object/tool attachment。
3. mesh 提取可暂时留在 `RobotViewerCore`，但 COACD 调用和写文件改为新服务。
4. GUI 参数面板提供：
   - `threshold`
   - `maxConvexHull`
   - `sampleResolution`
   - `mctsIteration`
5. COACD 任务放后台线程，完成后通过 document service 应用 override 并触发刷新。

验收：

- GUI 中生成后立即显示。
- 可在 active collision model 中选择生成的 `CoACD` 结果。
- 保存项目后重新打开仍能显示。

### 阶段 4：删除旧重复实现

删除前检查：

```bat
rg "generateCoacdMeshes|writeObj|writeManifest|appGeneratedMeshPath|CollisionCoacdDecomposition" SMRobotApps SMRobotPlatform SMRobotCore
```

可删除目标：

- `RobotCollisionProxyGenerator.cpp` 中 COACD 专用的私有写文件/hash/manifest 逻辑。
- 被新服务替代的直接 COACD 调用逻辑。

保留目标：

- 非 COACD 的 box/sphere/sphere cover proxy 生成逻辑。
- runtime mesh 提取逻辑，直到有更好的 Platform 服务接管。

## 10. 最终建议

短期建议：不要再在 `RobotQtViewer` 里继续堆 COACD 逻辑。先把当前成功案例沉淀成 `SMRobotCore::Collision` 的稳定核心 API，再让 GUI 调它。

中期建议：把“生成项目资产和 override”的逻辑从 `RobotViewerCore` 中抽成 Platform 服务，保证 Qt、GLFW、SDK、回归测试都能复用。

长期建议：如果二进制部署确实希望完全隐藏 CoACD DLL，再评估静态链接 COACD。但当前更重要的是先把 Core API、项目资产生成、GUI 刷新链路分清楚并测试稳定。

## 11. 本次执行进展

已经完成：

- `CollisionCoacdResult` 增加输入/输出统计和 warnings 字段。
- `CollisionCoacdOptions` 增加 `fastPreviewDefaults()`，作为 GUI/示例/回归测试的快速默认参数。
- 新增 `CollisionCoacdAssetWriter`，集中负责：
  - 调用 `CollisionCoacdDecomposition`；
  - 生成稳定 hash；
  - 写出 OBJ part 文件；
  - 写出 manifest；
  - 返回 `appGenerated://collision/coacd/...` URI。
- `RobotCollisionProxyGenerator` 已迁移为调用 `CollisionCoacdAssetWriter`，不再自己维护 COACD hash、OBJ 写出、manifest 写出和 URI 拼装逻辑。
- `CoacdDecompositionTests` 已新增资产写出 smoke test，验证 manifest、OBJ 文件和 URI 生成。

仍未完成，作为下一阶段 GUI 工作：

- GUI 参数面板尚未暴露 `threshold`、`maxConvexHull`、`sampleResolution`、`mctsIteration`。
- GUI 调用仍是同步接口形态，后续需要放入后台任务，避免大模型分解阻塞界面。
- 生成完成后的 document mutation、active model selection、viewport/tree/panel 刷新链路需要继续按 document-view 结构梳理。
- 旧的 mesh 提取逻辑仍暂留 `RobotViewerCore`，后续可在确认边界后再抽成更稳定的 Platform 服务。
