# RobotQtViewer smoke test 与动态库 gate

## 本轮结论

本轮不新增 GUI smoke test，不把新模块改成动态库。

原因：

- 当前改动主要是目录和 target 边界调整。
- 各模块仍是 RobotQtViewer 内部模块，不是对外稳定 API。
- Windows 动态库化需要导出宏、安装规则、运行时 DLL 部署和 ABI 边界，不应在模块接口未稳定前执行。

## 已验证内容

已通过 Release 构建：

```text
cmake --build build --config Release --target RobotQtViewer
```

该验证覆盖：

- CMake target 关系。
- Qt moc/uic 生成。
- 静态库链接。
- 最终 `RobotQtViewer` 可执行程序生成。

## 建议新增的第一批 smoke test

后续建议优先增加无窗口 smoke test，而不是直接做 GUI 自动化。

候选：

- ToolSetup ViewModel builder：输入 `ProjectDocument`，验证 mount/attachment/asset 展示。
- SceneExplorer ViewModel builder：输入 `ProjectDocument` 和 runtime robot/object 列表，验证 tree node。
- Collision selection set command：输入 `ProjectDocument`，验证 selection set 增删改。
- Collision detector command：输入 `ProjectDocument`，验证 detector enable/visible/properties。
- Project document command：验证 robot/object/tool attachment 的 add/update/remove。

## 动态库化 gate

仅当满足以下条件时，才考虑把模块从 `STATIC` 改为 `SHARED`：

- 模块不依赖 RobotQtViewer 私有 shell。
- 模块 public header 稳定。
- 模块有清晰 export macro。
- 模块有安装规则。
- 模块可被最小外部 Qt consumer 编译链接。
- 模块有独立 smoke test。

当前所有新增模块保持 `STATIC`。

