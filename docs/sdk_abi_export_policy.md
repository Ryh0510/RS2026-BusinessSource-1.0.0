# SDK ABI 导出策略

本文档记录 Core/Platform component DLL 的正式 ABI 导出规则。它适用于后续 prebuilt SDK 中所有准备交付给第三方的 package/component。

## 总原则

- 新增或迁移到 DLL 的 component 必须使用显式 export header，不再以 `WINDOWS_EXPORT_ALL_SYMBOLS` 作为正式交付策略。
- 默认采用 method/function 级导出，只导出第三方需要调用、继承或销毁的 ABI 面。
- `WINDOWS_EXPORT_ALL_SYMBOLS` 只允许作为历史迁移期状态保留；保留时必须有 inventory、审计报告和后续迁移计划。
- ABI 规则服务于 package/component SDK，而不是单一 facade DLL。第三方可以 `find_package(... COMPONENTS ...)` 消费多个 package 下的多个 component target。

## Export header 规则

每个 DLL component 应拥有独立 export header：

```cpp
#pragma once

#if defined(_WIN32)
#  if defined(COMPONENT_BUILDING_DLL)
#    define COMPONENT_API __declspec(dllexport)
#  else
#    define COMPONENT_API __declspec(dllimport)
#  endif
#else
#  define COMPONENT_API
#endif
```

命名约定：

- header：`include/<Component>/<Component>Export.h`。
- API 宏：`<COMPONENT>_API`。
- build 宏：`<COMPONENT>_BUILDING_DLL`。
- CMake target 使用 `target_compile_definitions(<target> PRIVATE <COMPONENT>_BUILDING_DLL)`。

如果历史 component 已存在兼容宏，例如 `CameraCore` 的 `CAMERACORE_API`，可以保留原宏名，但后续新增导出面仍遵守本规则。

## Method/function 级导出规则

默认推荐：

- 给 public 构造、析构、成员函数、static 工厂函数、自由函数单独加 `<COMPONENT>_API`。
- 不给 private/protected helper 加 export。
- 不把内部 helper class、manager singleton 的私有构造函数、资源加载细节暴露为 ABI。
- 不因为一个类有少量 public API 就整体 `class COMPONENT_API Foo`，除非该类整体就是有意交付的 ABI 类型。

适合 method/function 级导出的情况：

- 类中存在大量 private helper。
- 类中包含 STL/Eigen/GL resource 成员。
- 只希望交付少量 stable 操作。
- 该类型未来可能需要保留源码内部自由重构空间。

可以使用 class-level export 的情况：

- 接口类或纯抽象类，且虚析构需要跨 DLL 边界可见。
- 小型稳定值类型，所有 public 成员都属于 ABI 承诺。
- 工厂/服务类的所有 public 方法都明确是 SDK 面，并且 private helper 不会被自动导出。

禁止作为正式 ABI 的情况：

- private/protected helper 被导出。
- copy constructor / copy assignment 因自动导出或 class-level export 无意成为 ABI。
- STL 容器、模板实例、OpenGL/第三方资源管理细节被批量导出。
- Debug/Release 或 dx64/rx64 下 ABI 面不受控地分叉。

## 审计 gate

正式发布应同时执行两类检查：

1. baseline 比对：`cmake/CheckSmRobotSdkExportSnapshot.cmake` 确认导出符号快照与已确认 baseline 一致。
2. suspicious export 审计：`cmake/AuditSmRobotSdkExportSnapshot.cmake` 自动发现疑似 private/protected helper、copy constructor、copy assignment 和过大导出面。

已完成显式导出迁移的 component 应使用 strict gate：

```powershell
cmake -DSMROBOT_SDK_ABI_AUDIT_SNAPSHOT:FILEPATH=cmake\sdk_abi_baselines\SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_ABI_AUDIT_DLL_REGEX:STRING="SimulationRuntime|SensorSimulation" -DSMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS:BOOL=ON -P cmake\AuditSmRobotSdkExportSnapshot.cmake
```

尚处于自动导出迁移期的 component 应使用 non-failing inventory：

```powershell
cmake -DSMROBOT_SDK_ABI_AUDIT_SNAPSHOT:FILEPATH=cmake\sdk_abi_baselines\SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_ABI_AUDIT_DLL_REGEX:STRING="RenderCore|SceneCore|RobotRenderBridge" -DSMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS:BOOL=OFF -P cmake\AuditSmRobotSdkExportSnapshot.cmake
```

## 迁移准入规则

一个 component 从自动导出迁移到正式 DLL ABI 前，至少需要满足：

- 已列出 public headers、public/private link dependencies、导出符号数量和 suspicious findings。
- 已明确哪些类/函数属于 SDK ABI，哪些只是源码内部实现。
- 已确认是否允许直接暴露 STL/Eigen 类型；如果不允许，应先设计 facade 或数据传输结构。
- 已有 package consumer smoke test。
- strict audit 对该 component 的 DLL section 通过或有明确 baseline 豁免。

## 重渲染链路特别规则

`SceneCore`、`RenderCore`、`RobotRenderBridge` 当前仍处于自动导出状态。它们牵涉 OpenGL resource、scene graph、robot/runtime/collision visualization 边界，不能直接按普通轻量 component 批量迁移。

在它们进入迁移前，必须先完成：

- ABI inventory 稳定；
- public/private dependency 边界确认；
- 是否采用 facade 的决策；
- 至少一个最小 consumer smoke 设计；
- 对 dx64/rx64 导出差异的解释。

