# ProjectDocument 修改入口收口计划

## 目标

- 将 `RobotQtViewer` 中长期项目数据的写入统一收口到明确的 mutation/service/facade 入口。
- 让 dirty 标志只在真实用户编辑成功后置位；打开、加载兼容归一化、预览、运行时刷新不应误置 dirty。
- 逐步减少 UI controller 直接取得可写 `ProjectDocument&` 后修改字段的路径，为后续撤销/事务/审计打基础。

## 非目标

- 本轮不修改 project json schema。
- 本轮不引入新依赖。
- 本轮不重命名核心数据结构字段。
- 本轮不重写 ToolSetup、Collision、SceneExplorer 的 UI 架构，只收口持久化写入口。

## 文件检查范围

- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerDocumentController.*`
- `SMRobotApps/RobotQtModules/Shared/RobotQtViewerDocumentContext.*`
- `SMRobotApps/RobotQtModules/Shared/SceneEntityWorkflowController.*`
- `SMRobotApps/RobotQtModules/SceneExplorer/SceneExplorerModuleController.*`
- `SMRobotApps/RobotQtModules/ToolSetup/ToolSetupModuleController.*`
- `SMRobotApps/RobotQtModules/MotionControl/MotionControlModuleController.*`
- `SMRobotApps/RobotQtModules/Collision*/*DocumentFacade.*`
- `SMRobotApps/RobotQtViewer/MainWindow.*`
- `SMRobotPlatform/SimulationProject/ProjectSession.*`
- `SMRobotPlatform/SimulationProject/ProjectDocumentService.*`

## 预计修改文件

- 主要修改 `RobotQtViewerDocumentController.*`，作为统一 mutation 门面。
- 迁移 Shared / SceneExplorer / ToolSetup / Motion / Collision 中直接写入 document 和直接 `markDirty()` 的路径。
- 必要时给 collision workbench services 增加 mutation 能力，但不改变 collision 核心语义。
- 更新 `docs/agent_change_audit.md` 记录本轮阶段结果。

## 可能变化的类和函数

- `RobotQtViewerDocumentController::mutateProject(...)`
- `SceneEntityWorkflowController` 的导入、删除、transform、回滚路径
- `SceneExplorerModuleController` 的 ObjectFrame 创建/修改路径
- `ToolSetupModuleController` 的 mount、attachment、asset、binding、preview、apply/cancel 路径
- `MotionControlModuleController::apply...`
- Collision document facade 的 set/sync/export/request 更新路径

## 预期行为

- 未修改项目时执行 New/Open 不弹保存提示。
- 真正导入、移动、绑定、解绑、修改 mount/object frame/collision/motion 后 dirty 置位。
- 无变化的 Apply 不置 dirty。
- 预览可以刷新 viewport，但不改变 dirty。
- 加载兼容迁移可以改变内存归一化结果，但不触发“用户修改”语义。

## 验证方法

```bat
git -c safe.directory=D:/program/src/RS2026_CodexDev diff --check
cmake --build build --config Release --target RobotQtViewer
```

手工验证建议：

- 启动后不修改，直接 Open 另一个 project，不弹保存提示。
- 修改 Object/Mount/ObjectFrame 后 Open，弹保存提示。
- 执行 binding preview 后 Cancel，dirty 不变化。
- 执行 binding Apply / unbind 后 dirty 置位。
- Collision selection/detector/request 修改后 dirty 置位。

## 为什么这是最小方案

- 不改 schema，不新增依赖，不移动模块。
- 保留既有 `ProjectDocumentService` 和各 workbench facade，只把 dirty policy 和 publish 行为集中。
- 不一次性重写 UI，而是按已有 controller/facade 的自然边界迁移写入口。

## 阶段划分

1. 建立统一 mutation API，并先迁移 Shared 场景实体工作流。
2. 迁移 SceneExplorer 的 ObjectFrame 和 transform 写入。
3. 迁移 ToolSetup 的 mount/attachment/asset/binding 写入和预览回滚。
4. 迁移 MotionControl 的 robot 初始状态写入。
5. 迁移 Collision document facade 的写入和 dirty 策略。
6. 收紧公开可写入口，保留只读 `document()`，将必须可写的路径限定到 mutation/service 内。
7. 构建验证并更新审计文档。

## 停止条件

- 需要修改 project json schema。
- 需要引入新依赖。
- 需要跨模块重命名公共 API。
- 连续两次构建失败且失败原因不同。
- 发现某个模块必须大规模重写才能保持行为。
