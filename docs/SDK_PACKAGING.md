# SMRobotCore SDK Packaging

## Public Package Shape

The primary SDK package exports only:

- `SMRobotCore::RobotSDK`

The compatibility export set also includes:

- `SMRobotCore::RobotTrajectoryCore`

Other SMRobotCore targets are internal implementation details and should not
be consumed by external SDK users.

See `SDK_EXTENSION_BOUNDARIES.md` before adding project-level or visualization
SDK targets. Those targets must stay separate from the core `RobotSDK` facade.

## Installed Layout

The expected SDK install layout is:

```text
SMRobotCore/
  bin/
    RobotSDK*.dll
  lib/
    RobotSDK*.lib
    RobotTrajectoryCore*.lib
    cmake/
      SMRobotCore/
        SMRobotCoreConfig.cmake
        SMRobotCoreConfigVersion.cmake
      RobotSDK/
        RobotSDKTargets.cmake
        RobotSDKDependencies.cmake
      RobotTrajectoryCore/
        RobotTrajectoryCoreTargets.cmake
        RobotTrajectoryCoreDependencies.cmake
  include/
    RobotSDK/
  examples/
    QuickStart/
    CollisionQuickStart/
    PackageConsumer/
  docs/
    SDK_API.md
    SDK_EXTENSION_BOUNDARIES.md
    SDK_PACKAGING.md
```

## Build And Install

Configure, build, and install the selected configuration:

```powershell
cmake -S . -B build -DUSER_TANGTANG_p15v3_Windows=ON -DBuildExample=ON
cmake --build build --config Release
cmake --install build --config Release --prefix build/install
```

`cmake --install` does not build missing targets. Always run
`cmake --build` first, or build the generated `INSTALL` target from Visual
Studio.

## Consuming The Installed Package

Use `CMAKE_PREFIX_PATH` to point CMake to the installed `SMRobotCore`
directory:

```powershell
cmake -S path\to\PackageConsumer -B path\to\PackageConsumer\build -DCMAKE_PREFIX_PATH=path\to\install\SMRobotCore
cmake --build path\to\PackageConsumer\build --config Release
```

The consumer CMake should request only `RobotSDK`:

```cmake
find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotSDK)

target_link_libraries(MyApp
    PRIVATE
        SMRobotCore::RobotSDK
)
```

## Runtime Files

Applications need `RobotSDK*.dll` beside the executable or available in
`PATH`. The `PackageConsumer` example copies the imported SDK DLL beside the
example executable after build.

Third-party runtime DLLs required by the robot loader or collision backend
must also be distributed according to their licenses.

## Compatibility Notes

The SDK public ABI is controlled by `getSdkAbiVersion()`. Increase the ABI
version whenever a virtual SDK interface changes.

External users should not depend on internal targets, internal headers, or
static library filenames. Those details may change without an SDK ABI break.
