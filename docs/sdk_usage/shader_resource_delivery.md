# Shader 资源 DLL 使用说明

## 背景

`RenderCore` 默认 shader 原来从 `data/shader_gen3` 目录读取。为了让 SDK 和最终可执行程序发布更轻量，默认 shader 现在可以由 `RenderCoreShaderResources` 动态库提供。

这意味着常规发布不再需要携带 `data/shader_gen3`。该目录仍然保留给源码树开发和 shader 调试使用。

## 默认使用方式

应用仍然调用原有入口：

```cpp
rendercore::ShaderLibrary::initialize();
```

默认解析顺序为：

```text
1. 手动注册的 shader source provider。
2. 显式传入的 provider。
3. exe 同目录或 RenderCore DLL 同目录下的 RenderCoreShaderResources 动态库。
4. SMROBOT_SHADER_ROOT 指定的外部 shader 目录。
5. DATA_PATH/shader_gen3。
6. PROJECT_SOURCE_PATH/data/shader_gen3。
```

第 5、6 项只是开发态 fallback。SDK 和 exe 发布包不应依赖源码树路径。

## 源码树开发

正常构建 `RenderCore` 时，会同时构建 `RenderCoreShaderResources`：

```powershell
cmake --build build --config Release --target RenderCore
```

两者会输出到同一类目录：

```text
build/Release/bin/
  RenderCore_shared_rx64.dll
  RenderCoreShaderResources_shared_rx64.dll
```

因此源码树开发时，即使临时移走 `data/shader_gen3`，默认 shader 也应能从资源 DLL 中加载。

## SDK Consumer

使用 installed SDK 的第三方工程建议：

```cmake
find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS RenderCore SceneCore RobotRenderBridge)

target_link_libraries(MyRobotApp PRIVATE
    SMRobotPlatform::RenderCore
    SMRobotPlatform::SceneCore
    SMRobotPlatform::RobotRenderBridge
)

smrobot_copy_runtime_dependencies(MyRobotApp)
```

`smrobot_copy_runtime_dependencies()` 会把 `SMRobotPlatform/bin` 下的运行时 DLL 复制到 `MyRobotApp.exe` 输出目录，其中包括：

```text
RenderCoreShaderResources_shared_rx64.dll
```

如果 consumer 自己已有运行时复制规则，也可以手动复制该 DLL。

## 可执行程序发布

推荐发布结构：

```text
MyRobotApp/
  bin/
    MyRobotApp.exe
    RenderCore_shared_rx64.dll
    RenderCoreShaderResources_shared_rx64.dll
    其他运行时 DLL
  data/
    robots/
    meshes/
    projects/
```

默认发布包不需要：

```text
data/shader_gen3
```

如果需要调试或替换 shader，可以额外提供外部目录，然后显式初始化：

```cpp
rendercore::ShaderLibrary::initializeFromDirectory("shaders/shader_gen3");
```

## 自定义 Provider

第三方可以把 shader 放入自己的资源系统，然后注册 provider：

```cpp
auto provider = std::make_shared<MyShaderSourceProvider>();
rendercore::ShaderLibrary::registerShaderSourceProvider(provider);
rendercore::ShaderLibrary::initialize();
```

`MyShaderSourceProvider` 需要实现：

```cpp
rendercore::IShaderSourceProvider
```

该方式适合自定义压缩包、数据库、插件 DLL 或热更新资源系统。

## 交付检查

发布前至少检查：

```text
源码树运行：移走 data/shader_gen3 后 viewer/probe 能启动。
SDK consumer：只设置 CMAKE_PREFIX_PATH 到 installed SDK，consumer 能构建和运行。
exe 发布包：临时目录内无源码树、无 data/shader_gen3，exe 能启动。
```
