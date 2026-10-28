# Common

`Common` provides logging, utility, OpenGL runtime, image loading, and license verification libraries used by the robot SDK packages. This local prebuilt package contains public headers, configuration-specific libraries, CMake Config/Targets files, and required runtime DLLs.

Use the installed prefix with CMake:

```cmake
find_package(Common CONFIG REQUIRED COMPONENTS Utility)
target_link_libraries(MyApplication PRIVATE Common::Utility)
```

See `docs/getting-started.md` and `examples/UtilityQuickStart`. Third-party development dependencies such as Eigen and glm are resolved from your CMake environment; they are not copied as a top-level `thirdparty` directory by this local installer. This directory is for local validation, not a release artifact.
