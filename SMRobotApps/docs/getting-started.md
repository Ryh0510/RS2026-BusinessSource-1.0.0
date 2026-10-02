# RobotViewerCore 接入说明

## 环境要求

- CMake 3.20 或更高版本；
- 支持 C++17 的编译器；
- 与 SDK 根 manifest 一致的体系结构和兼容工具链；
- SDK 根目录中的 `Common`、`SMRobotCore`、`SMRobotPlatform` 和 `SMRobotApps` 保持相对位置不变；
- external thin profile 所需的 third-party 和 data root 由消费工程显式提供。

## CMake 消费

将完整 SDK 根目录加入 `CMAKE_PREFIX_PATH`：

```cmake
find_package(SMRobotApps CONFIG REQUIRED COMPONENTS RobotViewerCore)

add_executable(MyViewer main.cpp)
target_compile_features(MyViewer PRIVATE cxx_std_17)
target_link_libraries(MyViewer PRIVATE SMRobotApps::RobotViewerCore)
```

不要直接链接 `lib/` 中的具体文件。CMake imported target 会根据 Debug/Release 配置选择匹配的静态库，并传递公开 include 和依赖。

## 示例验证

```powershell
cmake -S examples/RobotViewerCoreQuickStart -B build/example -DCMAKE_PREFIX_PATH=<sdk-root>
cmake --build build/example --config Release
ctest --test-dir build/example -C Release --output-on-failure
```

## 自定义平面与曲面

`RobotViewport` 提供 `VisualizationSDK/CustomMesh.h` 定义的自定义三角网格接口。网格可以使用统一颜色或逐顶点 RGBA；任意曲面需要先离散为三角形，也可以使用规则高度场生成器。

```cpp
#include <RobotViewport.h>
#include <VisualizationSDK/CustomMesh.h>

using namespace smrobot::visualization;

CustomMeshDesc desc;
desc.ownerId = "my-extension";
desc.meshId = "preview-surface";

PlaneMeshSpec plane;
plane.width = 2.0f;
plane.height = 1.0f;
CustomMeshResult result = makePlaneMesh(plane, desc.mesh);
if(!result.success) {
    // Report result.message to the caller.
}

desc.mesh.colors.assign(
    desc.mesh.positions.size(),
    Color4f{ 0.15f, 0.65f, 0.95f, 0.55f });
desc.appearance.shading = MeshShadingMode::Unlit;
desc.appearance.blend = MeshBlendMode::AlphaBlend;
desc.appearance.cull = MeshCullMode::None;

CustomMeshHandle handle;
result = viewport.upsertCustomMesh(desc, handle);
```

调用 `upsertCustomMesh()` 前，`RobotViewport` 必须已经显示并完成 OpenGL 初始化。相同 `(ownerId, meshId)` 会更新原对象并保留 handle。插件或业务模块退出时应调用 `clearCustomMeshes(ownerId)`；单对象也可更新 geometry、color、transform、appearance、visible 或按 handle 删除。

完整的安装后示例位于 `examples/CustomMeshViewerQuickStart`。自定义网格属于临时可视化贡献，不写入项目文件，也不会自动加入碰撞世界。透明渲染采用对象级排序；单个自交透明网格仍可能受三角形顺序影响。

构建示例后，可使用人工展示模式观察半透明平面和高度场，关闭窗口即可退出：

```powershell
.\CustomMeshViewerQuickStart.exe --interactive
```

Windows 运行时仍需部署 `Common`、`SMRobotCore` 和 `SMRobotPlatform` 动态组件所需 DLL；`RobotViewerCore` 自身不提供 DLL。
