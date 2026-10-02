# SMRobotApps SDK

`SMRobotApps` 的开发 SDK 提供不依赖 Qt 应用壳的 `RobotViewerCore` 静态库。第三方工程应通过 CMake imported target 使用本包，不应手工拼接头文件或 `.lib` 路径。

## 包内容

- `include/RobotViewerCore/`：公开头文件；
- `lib/`：Debug/Release 静态库和 CMake package config；
- `docs/`：接入和依赖说明；
- `examples/`：从安装包独立构建的最小示例；
- `manifest.json`：包名、版本、公共 component 和结构约束。

## 快速开始

```cmake
find_package(SMRobotApps CONFIG REQUIRED COMPONENTS RobotViewerCore)
target_link_libraries(MyViewer PRIVATE SMRobotApps::RobotViewerCore)
```

完整步骤见 `docs/getting-started.md`，可运行示例位于 `examples/RobotViewerCoreQuickStart`。
