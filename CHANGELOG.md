# Change Log

本文档记录 RS2026 各正式版本中面向使用者和集成方的重要变更。

## [1.0.3] - 2026-08-25

### 新增

- 建立 `project://`、`data://`、`smrobot-asset://` 资产协议及项目伴随资产目录，支持项目迁移、校验和事务式另存为。
- 增加场景对象、机器人链接和挂载附件的统一碰撞模型生产流程，以及 CoACD 不可变 revision 和 current selection 管理。
- 增加 Product Profile、Workbench 组合与事务式生命周期，支持运行时中英文切换和结构化操作状态历史。
- Windows 与 Ubuntu 增加 SDK、Business Source、Runtime 发布编排、ABI snapshot、消费者矩阵和归档校验。

### 变更

- RobotIO 统一解析 URDF 相对资源、`package://`、`package.xml` 和显式搜索目录。
- RobotQtViewer 完善全屏、点云导入、文件对话框策略、任务面板与控件视觉层级。
- 优化模型 CPU/GPU 缓存和场景资源释放，减少重复解析与 Geometry 构建。
- Business Source 只发布业务源码和所需 prebuilt，不再内嵌 `data/`；运行数据由部署环境通过 `SMROBOT_DATA_ROOT` 提供。

### 修复

- 修复普通保存、另存为、碰撞模型持久化、旧凸包清理、Windows 瞬时文件占用和长路径 staging 问题。
- 修复工具绑定后源场景对象继续参与碰撞而产生重复碰撞实体的问题。
- 修复 Business Source 独立 Debug/Release Viewer 构建后的 CoACD runtime variant 部署。
- 修复 Verification Developer Kit 兼容版本选择及旧缓存迁移。

### 发布与兼容性

- 项目、SDK、Business Source 和 Runtime 版本统一为 `1.0.3`。
- C++ 标准保持 C++17，未新增第三方依赖。
- SDK ABI 基线升级到 v6；既有公开 API 未发现移除或改名。
- Business Source 必须包含本 Change Log，且禁止包含 `data/`、`AGENTS.md`、Git 元数据、调试符号和内部授权组件。

## [1.0.2]

- 上一个发布基线。更早版本的详细变更未在当前仓库中形成可核验的统一 Change Log，因此不追溯补写。
