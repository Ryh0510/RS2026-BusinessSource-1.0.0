# RobotQtViewer 当前模块到 Workbench 的映射

## 当前映射

| 当前模块 | 未来角色 | 说明 |
| --- | --- | --- |
| SceneExplorer | ScenePanel | 主浏览树，可根据 task 切换选择/过滤模式。 |
| ToolSetup | ToolSetupWorkbench | 第一个任务工作台试点，后续承担 mount、tool、TCP、attachment 编辑。 |
| CollisionInspector | CollisionModelWorkbench + CollisionRequestWorkbench + CollisionResultView | 需要拆分，不应继续作为一个巨大 Inspector。 |
| MotionControl | MotionWorkbench | 关节控制、初始姿态、轨迹预览。 |
| StatusPanel | Shell summary | 默认状态摘要，不是复杂 task。 |

## 近期迁移策略

第一阶段不删除旧 tab 功能，而是将右侧区域改为 `TaskPanelStack`：

- Browse 显示 StatusPanel。
- Motion 显示 MotionControlWidget。
- ToolSetup 显示 ToolSetupWidget。
- Collision 暂时显示现有 CollisionInspectorPanel。

这样可以先建立进入/退出 task 的模型，再逐步拆分 ToolSetup 和 Collision 的内部页面。

## 平级化影响

未来 `RobotQtModules` 不应按 Inspector tab 平级化，而应按 task/workbench 平级化：

```text
RobotQtModules/
  Shared/
  Workbench/
  ToolSetupWorkbench/
  SceneExplorer/
  CollisionModelWorkbench/
  CollisionRequestWorkbench/
```

