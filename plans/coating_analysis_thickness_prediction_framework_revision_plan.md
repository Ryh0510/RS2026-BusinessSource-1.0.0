# Coating Analysis 厚度预测框架修订方案

更新时间：2026-07-21

## 1. 目标

在 `RobotQtViewer` 的 `Coating Analysis` 工作台中形成第一阶段可运行闭环：

1. 右侧面板提供 `Open Model` 按钮，选择网格模型并作为 `workpiece` 写入当前项目文档。
2. 文档变更后通过既有 viewport reload 流程把模型显示在主视图，而不是由 widget 直接创建场景节点。
3. 右侧面板提供 `Thickness Prediction` 按钮，调用 `SMRobotSpray::SprayThicknessPrediction` 中新增的演示预测入口。
4. 演示预测为模型表面采样生成确定性的厚度渐变；第一阶段不实现真实喷枪、轨迹和沉积物理模型。
5. 将厚度标量通过通用色图转换为逐顶点颜色，并作为可开关的 viewport surface-scalar overlay 显示。
6. 右侧面板显示垂直厚度色标，包含最小值、中间值、最大值和单位。
7. `Show Thickness` 复选框控制鼠标悬停厚度探测；勾选后在命中表面时显示插值厚度值，取消勾选后不执行探测并立即清除提示。

## 2. 非目标

- 不实现真实喷涂厚度求解、喷枪标定、轨迹采样、遮挡、反弹、雾化或材料物理模型。
- 不在第一阶段保存预测结果、色图范围或 `Show Thickness` 状态到项目文件，不修改 project schema。
- 不新增第三方依赖，不引入 C++20。
- 不把厚度业务逻辑、网格着色或拾取计算写入 `MainWindow`、panel widget 或 shader 特判。
- 不为厚度分析复制一套项目对象导入、selection 或 viewport reload 流程。
- 不把当前结果导出为新网格文件；结果导出属于后续阶段。

## 3. 当前代码事实

### 3.1 Workbench 与右侧面板

- `RobotQtViewerWorkbenchKind::CoatingAnalysis`、工具栏 action 和 package registry 已存在并可进入。
- `Coating Analysis` 当前仍映射到 `RobotQtViewerRightPanelKind::Status`，没有专用 panel、controller 或 view model。
- `SimWorkbench/SMRobotWorkbenchPaintingAnalysis/PaintingAnalysisWorkbench` 目前只是 `INTERFACE` target，只声明依赖 `WorkbenchCommon` 和 `SprayThicknessPrediction`，没有实现文件。
- `RobotQtViewer` 当前未把 `SMRobotWorkbenchPaintingAnalysis::PaintingAnalysisWorkbench` 列为必需 target，也没有创建该工作台的面板实例。

### 3.2 模型导入与视口刷新

- 通用对象导入已经由 `SceneEntityWorkflowController::importSceneObjectFromPath(...)` 通过 `ProjectSceneEntityCommands` 和 `RobotQtViewerDocumentController::mutateProject(...)` 修改项目文档。
- `ViewportReloadWorkflowController` 已提供标准流程：发布 reload request、从 document 重建 viewport、发布 reload result。
- 现有 `MainWindow::importObject()` 包含文件选择、导入、viewport reload、失败回滚等 shell 编排；新工作台应复用 shared workflow，不应复制 document mutation 逻辑。
- 对象选择可以复用 `RobotQtViewerSelectionModel::selectSceneObject(...)`。

### 3.3 已有厚度与彩色模型能力

- `SMRobotSpray/SprayThicknessPrediction` 已有：
  - `ThicknessField`、`ThicknessPredictionResult`、`ThicknessMetrics`；
  - 真实入口 `SprayThicknessPredictor::predict(...)`；
  - `ThicknessVisualization::evaluateDemoBurnerThickness(...)`；
  - `ThicknessVisualization::mapThicknessToColor(...)`。
- `AssetCore::GeometryDesc` 已支持逐顶点 `colors`。
- `RenderCore::ModelManager` 已把 `GeometryDesc::colors` 写入 vertex attribute 3。
- `MLRobotUBO` shader 已把 `vVertexColor` 乘入 base color，因此现有渲染管线具备显示彩色网格的基础能力。
- `ProjectRuntimeBuilder` 当前对文件名为 `burner.stl` 且类型为 `workpiece` 的对象自动调用演示厚度着色。这条路径存在三个问题：
  1. 由文件名触发，不是 `Coating Analysis` 用户命令；
  2. 预测、色图和 scene build 混在一起；
  3. `AssetManager` 返回缓存的共享 `ModelDesc`，就地写入 colors 可能污染同一资产的其他使用者。

### 3.4 当前拾取能力

- `RobotViewport` 已开启 mouse tracking，但 `mouseMoveEvent(...)` 目前只处理相机拖动。
- `ProjectScene::pickScreenPoint(...)` 只为 scene object 使用球体/AABB 粗拾取，返回结果没有 triangle、命中点或重心坐标。
- 因此现有 click selection 可以复用，但不能直接满足“停留在任意表面位置显示该处厚度”；需要一个只面向 active scalar overlay 的精确表面 probe。

## 4. 不变量、所有权与架构判断

### 4.1 必须保持的不变量

- 导入模型是项目事实，必须由 document command 修改并可随项目保存。
- 厚度预测结果是第一阶段可重建的运行/分析结果，不属于 widget，也不进入 project schema。
- 厚度值是领域标量；颜色、色标、overlay 和 hover 是该标量的可视化投影。
- `RobotQtViewer` 只做 Qt shell 和服务适配；真实预测能够在无 Qt 测试中调用。
- 同一份色图定义必须同时驱动网格颜色和右侧 legend，不能各自硬编码。
- 启用 hover probe 时，数值必须来自与当前显示 overlay 相同的标量数组，并采用与 GPU 顶点颜色一致的三角形重心插值。

### 4.2 模块归属结论

| 能力 | 拥有模块 | 原因 |
|---|---|---|
| 演示厚度生成、真实厚度结果、统计量和厚度单位 | `SMRobotSpray::SprayThicknessPrediction` | 喷涂领域语义，可被优化器、SDK、headless 测试复用，不依赖 Qt/OpenGL |
| 通用标量归一化与色图采样 | `SMRobotPlatform::VisualizationSDK` | 颜色是可视化策略，不应成为喷涂预测的核心业务；该能力也可复用于温度、应力、距离等标量场 |
| 将通用 surface scalar overlay 应用到当前 scene object、恢复原模型、精确表面 probe | `SMRobotApps::RobotViewerCore` | 这是当前 viewer 对 SceneCore/RenderCore 的具体适配；接口保持通用，不引用 spray 类型 |
| Coating Analysis task/session、模型到 workpiece sample 的适配、预测编排、view model | `SMRobotWorkbenchPaintingAnalysis` | 属于该 workbench 的应用编排，但不拥有项目或 GPU 事实 |
| dock/panel 组合、顶层 signal 接线、status/tooltip 展示 | `RobotQtViewer` | `MainWindow` 只作为 shell/composer |

结论：不应把“厚度模型直接转成 GPU 模型”的全部流程放入 `SMRobotSpray`。`SMRobotSpray` 返回厚度标量；通用色图和 scene overlay 分别位于可视化层和 viewer bridge。现有 `ThicknessVisualization` 可保留兼容包装，但不再作为 `ProjectRuntimeBuilder` 的硬编码入口。

## 5. 第一阶段数据契约

### 5.1 演示预测 API

在 `SprayThicknessPrediction` 增加独立的占位预测器，不使用假喷枪或假轨迹调用真实 `SprayThicknessPredictor`：

```cpp
struct DemoThicknessPredictionOptions
{
    Eigen::Vector3d gradientDirection = Eigen::Vector3d::UnitZ();
    double minThicknessMeters = 20.0e-6;
    double maxThicknessMeters = 120.0e-6;
    double transverseWaveRatio = 0.10;
};

class DemoThicknessPredictor
{
public:
    static ThicknessPredictionResult predict(
        const sprayworkpiece::WorkpieceModel& workpiece,
        const DemoThicknessPredictionOptions& options = {});
};
```

行为：

- 将每个有效 surface sample 投影到 `gradientDirection`，按投影包围范围归一化。
- 以归一化投影为主项生成从 `minThicknessMeters` 到 `maxThicknessMeters` 的渐变。
- 可加入小幅横向正弦扰动，使任意普通模型上都能明显看到多色变化，但输出必须确定性、无随机数。
- 空模型、零方向、退化包围范围返回可诊断 warning，不产生 NaN/Inf。
- 领域内部统一使用米；Qt view model 统一转换为 `um` 显示。
- 输出继续使用既有 `ThicknessPredictionResult` 和 `ThicknessMetrics`，为以后替换真实 predictor 保持调用形态稳定。

### 5.2 网格采样与绑定

`PaintingAnalysisWorkbench` 增加 CPU-only adapter：

```text
AssetCore ModelDesc
    -> one SurfaceSample per render vertex
    -> WorkpieceModel
    -> MeshSampleBinding[subMeshIndex, vertexIndex, sampleIndex]
```

- 第一阶段一顶点一 sample，法线存在时复用法线，不存在时使用三角面累加生成顶点法线或退化为 `UnitZ` 并记录 warning。
- binding 显式保存 submesh/vertex 与 sample 的对应关系；禁止假设所有 submesh 已经扁平为一个顶点数组。
- `visualScale` 与 `ModelDesc::get_local()` 的选择必须一致：预测可在模型局部坐标完成，但 probe 必须使用同一 local-to-world 变换。

### 5.3 通用色图契约

在 `VisualizationSDK` 增加不依赖 Qt、OpenGL、spray 的通用结构：

```cpp
struct ScalarColorStop { double position; Eigen::Vector3f color; };
struct ScalarColorMap { std::vector<ScalarColorStop> stops; };
struct ScalarRange { double minimum; double maximum; };
```

提供 clamp、退化范围处理和线性插值。第一阶段默认 palette 使用蓝—青—绿—黄—红，并由同一对象同时生成：

- overlay 逐顶点 RGB；
- 右侧 legend 的 gradient stops；
- min/mid/max 标签对应值。

### 5.4 Viewer overlay 契约

`RobotViewerCore` 增加通用、无 spray 类型的输入与查询结果：

```cpp
struct SurfaceScalarSubMesh
{
    std::vector<double> values; // 与源 ModelDesc submesh positions 一一对应
};

struct SurfaceScalarOverlay
{
    std::string objectId;
    std::vector<SurfaceScalarSubMesh> subMeshes;
    ScalarRange range;
    ScalarColorMap colorMap;
    std::string quantityName;
    std::string unit;
};

struct SurfaceScalarProbeResult
{
    bool hit;
    std::string objectId;
    double value;
    Eigen::Vector3d worldPosition;
};
```

应用 overlay 时：

- 从 object document/source path 取得 CPU `ModelDesc`，先复制再写 colors，严禁修改 `AssetManager` 缓存对象。
- 用复制后的 desc 构建独立 render model，并令 analysis material 不受原纹理/base color 污染；优先使用白色 `Unlit` 材质，使 legend 与网格颜色一致。
- `RuntimeSceneObject` 保存 original model、overlay model 和 overlay scalar mesh；显示/隐藏只交换 `ModelNode::setModel(...)`。
- clear/reload/project replacement 时释放 overlay，恢复 original model。

## 6. 标准交互与事件流

### 6.1 Open Model

```text
CoatingAnalysisPanel::openModelRequested
    -> CoatingAnalysisModuleController
    -> PaintingAnalysisDialogService (QFileDialog)
    -> SceneEntityWorkflowController::importSceneObjectFromPath(path, "workpiece")
    -> ProjectDocumentService / ProjectSceneEntityCommands
    -> ViewportReloadWorkflowController::reload(...)
    -> ViewportReloaded event
    -> SelectionModel::selectSceneObject(newObjectId)
    -> CoatingAnalysisViewModel rebuild
    -> panel displays active model; viewport displays document object
```

- 文件过滤器复用当前对象导入支持的 STL/OBJ/DAE/PLY。
- reload 失败时复用 `SceneEntityImportResult` 快照回滚 document 和 dirty state。
- 打开新模型前清理旧 analysis result/overlay/tooltip；成功后 `Thickness Prediction` 才可用。

### 6.2 Thickness Prediction

```text
CoatingAnalysisPanel::predictionRequested
    -> controller resolves active SceneObjectDesc and portable asset path
    -> AssetManager loads CPU ModelDesc
    -> PaintingAnalysisMeshAdapter builds WorkpieceModel + binding
    -> DemoThicknessPredictor::predict(...)
    -> binding projects ThicknessField to per-submesh values
    -> shared ScalarColorMap + range
    -> PaintingAnalysisWorkbenchServices::applySurfaceScalarOverlay(...)
    -> RobotQtViewer adapter
    -> RobotViewport / ProjectScene runtime overlay
    -> CoatingAnalysisChanged event
    -> view model rebuilds metrics + legend + enabled states
```

- 预测是 `RuntimeOnly`/analysis state，不设置 project dirty。
- prediction 失败时保留原始模型，清除不完整 overlay，并在 panel/status bar 显示诊断。
- 真实算法接入时只替换 controller 使用的 predictor，不改变 panel、overlay、legend 或 probe 契约。

### 6.3 Hover thickness probe

```text
Show Thickness checked
    -> controller enables probe for active object
    -> RobotViewport mouseMoveEvent (throttled)
    -> ProjectScene::probeSurfaceScalarAtScreenPoint
    -> nearest triangle ray hit
    -> barycentric interpolation of three vertex scalar values
    -> surfaceScalarHovered(result, viewportPosition)
    -> controller formats value in um
    -> shell displays QToolTip near cursor and panel updates current-value label
```

- 未勾选、没有结果、当前 mode 不是 `Coating Analysis`、鼠标正在拖动相机或未命中 active object 时，不执行/不显示厚度 probe。
- 取消勾选、离开 mode、打开/关闭项目或清理结果时立即隐藏 tooltip 并清空 current-value label。
- 第一阶段只对 active analysis object 做 triangle test，并限制到约 30 Hz，避免每帧扫描全部场景。
- 三角求交采用 local-space Moller-Trumbore；按世界空间距离选择最近命中。若大模型性能不满足，再在后续阶段增加 BVH，不在本阶段引入第三方依赖。

## 7. 右侧面板设计

新增 `CoatingAnalysisPanel`，由 `CoatingAnalysisViewModel` 完整驱动：

1. `Model` 分组：当前模型名/路径摘要和居中的 `Open Model` 按钮。
2. `Prediction` 分组：居中的 `Thickness Prediction` 按钮和状态/警告文本。
3. `Thickness` 分组：
   - `Show Thickness` 复选框，默认未勾选，只控制 hover 数值探测；预测后的热力图本身保持显示。
   - 垂直 `ThicknessLegendWidget`，显示连续 gradient、max/mid/min 和 `um`。
   - `Current` 标签，命中时显示当前悬停值，未命中时显示 `--`。
4. 可选统计摘要：min、max、average；这些数据已经由 `ThicknessMetrics` 提供，不在 widget 中重复计算。

这里的“厚度进度条”实现为色标 legend，而不是 `QProgressBar`。它表达数值到颜色的映射，不表达预测任务完成百分比。

按钮状态：

| 状态 | Open Model | Thickness Prediction | Show Thickness | Legend |
|---|---:|---:|---:|---:|
| 无模型 | 可用 | 禁用 | 禁用 | 隐藏/空 |
| 模型已加载、无结果 | 可用 | 可用 | 禁用 | 隐藏/空 |
| 正在预测 | 禁用 | 禁用 | 禁用 | 保留旧结果或显示 busy |
| 结果可用 | 可用 | 可用 | 可用 | 显示 |
| 失败 | 可用 | 可重试 | 禁用 | 不显示不完整结果 |

## 8. Workbench 生命周期

- 进入 `Coating Analysis`：切换到专用 right panel；若当前 project session 仍有有效结果，则重新显示 overlay。
- 离开该 mode：关闭 hover probe、清 tooltip、恢复目标对象原始模型；结果可保留在本次 project 的 analysis session 中，便于再次进入时重显。
- `ProjectOpened`、new project、目标对象删除或 viewport reload 指向不同 document：清理 result/session/overlay。
- viewport 因同一 document 重建：controller 收到 `ViewportReloaded` 后，根据仍有效的 analysis session 重新应用 overlay。
- 第一阶段结果不保存；应用关闭或项目替换后必须重新预测。若以后需要保存/导出，应新增明确的 coating result document/runtime schema，而不是序列化 Qt 状态。

## 9. 拟修改文件与职责

以下是实施时的预期文件组；开始编码前仍需根据当时源码复核具体名称。

### 9.1 `SMRobotSpray/SprayThicknessPrediction`

- 新增 `include/SprayThicknessPrediction/DemoThicknessPrediction.h`
- 新增 `src/DemoThicknessPrediction.cpp`
- 调整 `ThicknessVisualization.h/.cpp`
  - 将 `evaluateDemoBurnerThickness` 的权威实现迁移到 `DemoThicknessPredictor`。
  - `mapThicknessToColor` 不再由 viewer runtime builder 直接使用；若公共 SDK 兼容性需要，保留窄 wrapper 并注明移除条件。
- 增加无 Qt 单元/回归测试，覆盖方向渐变、退化范围、空输入、单位范围和确定性。

### 9.2 `SMRobotPlatform/VisualizationSDK`

- 新增 `include/VisualizationSDK/ScalarColorMap.h`
- 新增 `src/ScalarColorMap.cpp`
- 如 overlay 公共数据需要跨模块共享，再新增 `SurfaceScalarVisualization.h`；保持无 Qt、无 spray、无 OpenGL 公共依赖。
- 增加 endpoint/midpoint/clamp/degenerate range 测试。

### 9.3 `SMRobotApps/RobotViewerCore`

- 新增 `SurfaceScalarOverlay.h/.cpp` 或等价集中实现，负责通用 overlay 数据验证、着色 desc 构建和 probe。
- 修改 `ProjectRuntimeTypes.h`：为 `RuntimeSceneObject` 增加显式 overlay runtime state，不把它混入 collision state。
- 修改 `ProjectScene.h/.cpp`：增加 apply/show/clear/probe API，并在 reload/clear 时回收状态。
- 修改 `RobotViewport.h/.cpp`：暴露 overlay API、probe enable API、hover signal，并在 `mouseMoveEvent(...)` 中做 gated/throttled probe。
- 修改 `ProjectRuntimeBuilder.cpp`：删除 `isDemoBurnerWorkpiece(...)`、`applyDemoBurnerThicknessColors(...)` 及 filename 特判。
- 修改 `RobotViewerCore/CMakeLists.txt`：删除 viewer 对 `SprayThicknessPrediction` 的演示硬依赖和 `ROBOT_VIEWER_CORE_HAS_SPRAY_THICKNESS`；按最终公共类型增加 `VisualizationSDK` 依赖。

### 9.4 `SimWorkbench/SMRobotWorkbenchPaintingAnalysis`

- 将 `PaintingAnalysisWorkbench` 从空接口包扩展为聚合 target，并新增例如 `CoatingAnalysis` 子 target。
- 新增：
  - `CoatingAnalysisPanel.h/.cpp`
  - `ThicknessLegendWidget.h/.cpp`
  - `CoatingAnalysisViewModel.h`
  - `CoatingAnalysisSession.h/.cpp`
  - `CoatingAnalysisModuleController.h/.cpp`
  - `PaintingAnalysisMeshAdapter.h/.cpp`
  - `PaintingAnalysisDialogService.h/.cpp`
  - `PaintingAnalysisWorkbenchServices.h`
- 修改 package `CMakeLists.txt`、`TargetConfigSetting.cmake`、`PackageConfigSetting.cmake`，声明 `WorkbenchCommon`、`SprayThicknessPrediction`、`AssetCore`、`SimulationProject` 和必要的通用 visualization 依赖。
- controller 通过 `RobotQtViewerDocumentViewRegistry` 订阅 `ProjectOpened`、`ProjectDocumentChanged`、`ViewportReloaded`、`SelectionChanged`、`TaskStateChanged` 和新增的 typed coating event。

### 9.5 `SMRobotApps/RobotQtModules/Shared`

- 修改 `RobotQtViewerWorkbench.h/.cpp`：新增 `RobotQtViewerRightPanelKind::CoatingAnalysis`，将 descriptor 从 `Status` 改为专用 panel。
- 修改 `RobotQtViewerEvents.h`：新增最小 typed `CoatingAnalysisChanged` 和 payload；不要通过字符串 message 携带结果结构。
- 修改 `RobotQtViewerWindowConfig.cpp`：注册 `coatingAnalysis` module 及其订阅。
- 若通用 overlay 不适合加入现有 `RobotQtViewerViewportServices`，保持 Shared 不引用 spray，使用 Painting Analysis 包自己的 services interface；推荐采用后一种方式。

### 9.6 `SMRobotApps/RobotQtViewer`

- 新增 `RobotQtViewerPaintingAnalysisServicesAdapter.h/.cpp`，只把 workbench services 调用转发给 `RobotViewport`。
- 修改 `MainWindow.h/.cpp`：
  - 创建 panel、controller、adapter；
  - 注册 module；
  - 将 panel 加入 `QStackedWidget`；
  - 在 right panel switch 中选择该 panel；
  - 连接 status message 与 hover tooltip 的顶层展示；
  - 不在 `MainWindow` 中计算厚度、色图或三角求交。
- 修改 `RobotQtViewer/CMakeLists.txt`：把 `SMRobotWorkbenchPaintingAnalysis::PaintingAnalysisWorkbench` 加入 required targets 和 link libraries。
- 若需双语，补充 `RobotQtViewerLanguage.cpp` 或由 panel view model 提供统一文本；不要把业务状态编码进翻译字符串。

## 10. 旧代码退役

### 可在本次直接删除

- `ProjectRuntimeBuilder` 中 `burner.stl` 文件名识别和自动厚度着色分支。
- `ROBOT_VIEWER_CORE_HAS_SPRAY_THICKNESS` 及仅为该分支存在的直接 Spray 依赖。

替代者：用户在 `Coating Analysis` 明确点击 `Thickness Prediction` 后，经 workbench controller 和通用 overlay service 触发。

### 迁移后再决定删除

- `ThicknessVisualization::evaluateDemoBurnerThickness(...)`：迁移调用者后检查全仓引用；若作为已导出公共 API 可能被外部 SDK 消费，则保留兼容 wrapper，内部路由到新 demo predictor，并记录版本化移除条件。
- `ThicknessVisualization::mapThicknessToColor(...)`：新的权威实现移到通用 color map；公共 API 同样先保留 wrapper，禁止新增调用者。

### 必须保留

- `SprayThicknessPredictor::predict(...)`：真实预测入口，优化器已有调用。
- `ThicknessField`、`ThicknessMetricsCalculator` 和 vertex color 渲染管线：均为 active code。

删除前必须使用 `rg` 检查源码、tests、examples、export headers 和 package consumer，随后构建相关 exported targets。

## 11. 分阶段实施顺序

### 阶段 A：领域占位 API 与通用色图

- 实现 `DemoThicknessPredictor` 和测试。
- 实现通用 `ScalarColorMap` 和测试。
- 保留兼容 wrapper，不接 GUI。

完成标准：无 Qt 测试能够从 `WorkpieceModel` 得到有限、确定、沿指定方向变化的厚度和一致 palette 颜色。

### 阶段 B：Viewer surface-scalar overlay

- 建立 overlay 数据、模型 clone/colorize/apply/restore 路径。
- 实现 triangle probe 与重心插值。
- 删除 burner filename patch。

完成标准：任意 scene object 可通过通用标量数组显示渐变、恢复原模型并查询准确表面值；无 spray 类型进入 ViewerCore 公共 API。

### 阶段 C：Painting Analysis workbench

- 实现 panel、legend、view model、session、controller、dialog 和 services interface。
- 复用现有 document import/reload/selection 流程。
- 增加 typed event 和 module subscriptions。

完成标准：controller fake-services 测试覆盖状态机、失败回滚、按钮 enabled 状态和 hover gate。

### 阶段 D：RobotQtViewer 接线与验收

- MainWindow 只完成组件创建、stack 切换、adapter 与 tooltip/status 接线。
- 执行全目标构建、smoke 和人工交互验收。

完成标准：用户闭环全部通过，切换 mode/project 不遗留 overlay 或 tooltip。

## 12. 验证方案

### 12.1 静态与构建验证

```bat
git -c safe.directory=D:/program/src/RS2026_CodexDev diff --check
cmake --build build --config Release --target SprayThicknessPrediction
cmake --build build --config Release --target VisualizationSDK
cmake --build build --config Release --target RobotViewerCore
cmake --build build --config Release --target CoatingAnalysisModule
cmake --build build --config Release --target RobotQtViewer
```

实际 build directory 应以实施时已配置且与当前源码一致的目录为准，不假定旧 `build` 一定有效。

### 12.2 自动测试

- Demo predictor：Z/任意方向单调性、min/max、空模型、flat mesh、invalid sample、确定性。
- Color map：端点、中点、多 stop、范围外 clamp、相等范围。
- Mesh adapter：多 submesh、缺法线、索引/非索引网格、sample binding 完整性。
- Overlay：应用后原 `AssetManager` 缓存 colors 不变；clear 后 original model 恢复。
- Probe：已知三角形的 vertex/edge/interior 命中值、最近命中、object transform、未命中。
- Workbench controller：无模型、导入成功/失败、预测成功/失败、reload、mode leave、checkbox gating。

### 12.3 人工 GUI 验收

1. 启动 `RobotQtViewer` 并进入 `Coating Analysis`。
2. 确认右侧显示专用 panel，初始只有 `Open Model` 可用。
3. 打开任意支持的 STL/OBJ/DAE/PLY，确认对象进入 document/tree 并显示在主视图。
4. 确认打开模型后 `Thickness Prediction` 可用，点击后显示连续彩色渐变。
5. 确认右侧 legend 的颜色、min/mid/max 与模型使用同一 range/palette，单位为 `um`。
6. 未勾选 `Show Thickness` 时，移动鼠标不显示数值。
7. 勾选后在多个表面位置停留，tooltip/current label 数值随位置连续变化。
8. 取消勾选后 tooltip 立即消失且不再探测。
9. 切换到其他 workbench 后恢复原模型外观；重新进入可恢复本项目会话内结果。
10. 打开新项目、删除目标对象或导入新分析模型后，旧 overlay/legend/tooltip 全部清理。

## 13. 风险与防护

- **共享资产缓存污染**：必须 clone `ModelDesc` 后写 colors；增加自动测试锁定该不变量。
- **多 submesh 对应错误**：使用显式 binding，不使用单一扁平 vertex index。
- **材质导致 legend 与模型色差**：analysis overlay 使用白色、无原纹理污染的材质，并人工比较 shader 输出。
- **hover 性能**：只探测 active object、约 30 Hz throttle；大模型超过预算时再引入内部 BVH。
- **结果生命周期混乱**：结果属于 `CoatingAnalysisSession`/viewer runtime，project replacement 和 mode exit 有明确清理路径。
- **公共 API 兼容**：现有 exported `ThicknessVisualization` 不直接删除，先变成窄 compatibility wrapper。
- **模块反向依赖**：`SMRobotSpray` 不依赖 `SMRobotPlatform`；由 Painting Analysis workbench 同时消费领域结果与通用 visualization contract。

## 14. 停止条件

实施时若出现以下任一情况，应先汇报并重新确认范围：

- 必须修改 project schema 才能完成第一阶段。
- 需要新增第三方依赖或改变 Core/Platform 包顺序。
- 通用 visualization 类型造成 `SMRobotSpray` 与 `SMRobotPlatform` 循环依赖。
- 为精确 probe 必须改写公共 RenderCore geometry API，且无法通过 overlay 自有 CPU mesh 避免。
- 预计修改超过 50 个 source/config 文件。
- 两次连续构建失败且根因不同，或无法证明 overlay 清理后原场景行为保持不变。

## 15. 第一阶段验收定义

- 支持用户显式打开任意受支持的网格模型并在主视图显示。
- `Thickness Prediction` 确实调用 `SMRobotSpray::SprayThicknessPrediction` 的独立占位预测 API，而不是在 Qt 回调中生成颜色。
- 模型显示明显、确定的厚度渐变；颜色与右侧 legend 一致。
- hover 数值只在 `Show Thickness` 勾选且命中 active result surface 时显示。
- project document、runtime、view model、widget 和 viewport 的所有权边界符合架构契约。
- 移除 `burner.stl` filename 特判和共享 `ModelDesc` 就地改色路径。
- 相关无 Qt 测试、工作台 controller 测试、目标构建和人工 smoke 均有实际通过证据。
