# SMRobotGen2 Package Examples

These examples are external package consumers. They are intentionally separate
from source-tree examples.

Configure with an installed package:

```powershell
cmake -S tests/package_examples -B build/package_examples -DCMAKE_PREFIX_PATH=D:/program/src/RS2026_CodexDev/PrebuiltPackages
cmake --build build/package_examples --config Release
```

Run the package authorization validation test:

```powershell
cmake -S tests/package_examples/license_authorization_consumer -B build/license_authorization_consumer -DCMAKE_PREFIX_PATH="D:/program/src/RS2026_CodexDev/PrebuiltPackages/Common;D:/program/src/RS2026_CodexDev/PrebuiltPackages/SMRobotCore;D:/program/src/RS2026_CodexDev/PrebuiltPackages/SMRobotPlatform;D:/program/src/RS2026_CodexDev/PrebuiltPackages/thirdparty/Windows/VerificationDeveloperKit-1.0.0" -DSMROBOTGEN2_PACKAGE_LICENSE_DIR=D:/program/src/RS2026_CodexDev/license
cmake --build build/license_authorization_consumer --config Release
ctest --test-dir build/license_authorization_consumer -C Release --output-on-failure
```

Run headless examples:

```powershell
.\build\package_examples\Release\SMRobotGen2SdkQuickStart.exe
.\build\package_examples\Release\SMRobotGen2CollisionCoreConsumer.exe
.\build\package_examples\Release\SMRobotGen2RobotIOUrdfConsumer.exe D:\program\src\RS2026_CodexDev\data\drake_models\franka_description\urdf\panda_arm_hand.urdf
.\build\package_examples\Release\LicenseAuthorizationConsumer.exe D:\program\src\RS2026_CodexDev\license
```

These examples must not include source-tree directories or link source-tree
targets directly.

## Core/Platform DLL package/component matrix

For the Boost-like package/component SDK route, configure these examples against
one installed SDK prefix:

```powershell
cmake -S tests/package_examples -B build/package_examples_d4 -G "Visual Studio 16 2019" -DCMAKE_PREFIX_PATH=D:/program/src/RS2026_CodexDev/build/sdk_dll_d4 -DSMROBOT_PACKAGE_EXAMPLES_BUILD_LEGACY_GEN2=OFF -DSMROBOT_PACKAGE_EXAMPLES_BUILD_LICENSE=OFF
cmake --build build/package_examples_d4 --config Release --target ProjectSimulationSdkConsumer SMRobotCoreRobotIOUrdfConsumer SMRobotPlatformComponentsSmoke
ctest --test-dir build/package_examples_d4 -C Release -R "ProjectSimulationSDK|RobotIO|AssetCore|CameraCore|SensorCore|ComponentSelection" --output-on-failure
```

The Core component DLL smoke target is included in the same configuration:

```powershell
cmake --build build/package_examples_d4 --config Release --target SMRobotCoreComponentsSmoke
ctest --test-dir build/package_examples_d4 -C Release -R "RobotCore|Kinematics|Collision|RobotTrajectoryCore|RobotRuntime|RobotInstance" --output-on-failure
```

The `SMRobotSDK.ComponentSelection` test creates temporary external CMake
projects and checks that requested components are imported precisely:

- `SMRobotPlatform COMPONENTS ProjectSimulationSDK` does not import
  `SMRobotPlatform::SimulationProject`.
- `SMRobotCore COMPONENTS RobotCore Kinematics RobotTrajectoryCore RobotIO`
  does not import `SMRobotCore::RobotRuntime`.
- `SMRobotCore::RobotCore`, `SMRobotCore::Kinematics`, and
  `SMRobotCore::RobotTrajectoryCore` have executable consumer smoke tests.
- `SMRobotCore::RobotRuntime` and `SMRobotCore::RobotInstance` have executable
  consumer smoke tests.
- `SMRobotCore::Collision` has an executable consumer smoke test.
- `SMRobotPlatform::AssetCore` has an executable consumer smoke test and does
  not expose Assimp as an installed public dependency.
- `SMRobotPlatform::CameraCore` has an executable consumer smoke test for the
  existing exported factory ABI.
- `SMRobotPlatform::SensorCore` has an executable consumer smoke test and does
  not import rendering components.
- `SMRobotPlatform::VisualizationSDK` has an executable consumer smoke test and
  imports `SimulationRuntime` without exposing `SceneCore`, `RenderCore`, or
  `RobotRenderBridge` as public package dependencies.
- A missing component fails at configure time with a clear error.

When validating source/prebuilt mixed builds from the main project, prefer an
explicit SDK root:

```powershell
cmake -S . -B build/p2 -G "Visual Studio 16 2019" -DUSER_TANGTANG_4090_Windows=ON -DBuildExample=ON -DSMROBOT_PREBUILT_PACKAGE_ROOT=D:/program/src/RS2026_CodexDev/build/sdk_dll_d4 -DUsingPrebuilt_SMRobotCore=ON -DUsingPrebuilt_SMRobotPlatform=ON
```

`SMROBOT_PREBUILT_PACKAGE_SELECTION_MODE` defaults to `Explicit`, so old
repository-local `PrebuiltPackages` directories are not used as the implicit
prebuilt root. Use `LegacyAuto` only when intentionally validating the old
workflow.

## SDK release hardening matrix

Install an SDK profile and write `SMRobotSDKManifest.json`:

```powershell
cmake -DSMROBOT_SDK_PROFILE_BUILD_DIR=build/e_final -DSMROBOT_SDK_PROFILE_PREFIX=build/sdk_e_final -DSMROBOT_SDK_PROFILE_CONFIGS="Release;Debug" -P cmake/InstallSmRobotSdkProfile.cmake
```

Validate the installed SDK with the repeatable consumer matrix:

```powershell
cmake -DSMROBOT_SDK_MATRIX_SDK_PREFIX:PATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final -DSMROBOT_SDK_MATRIX_BUILD_ROOT:PATH=D:/program/src/RS2026_CodexDev/build/i_platform_pkg_matrix -DSMROBOT_SDK_MATRIX_CONFIGS:STRING="Release;Debug" -DSMROBOT_SDK_MATRIX_GENERATOR:STRING="Visual Studio 16 2019" -DSMROBOT_SDK_MATRIX_ABI_BASELINE:FILEPATH=D:/program/src/RS2026_CodexDev/cmake/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_MATRIX_REQUIRE_ABI_BASELINE:BOOL=ON -P cmake/RunSmRobotSdkConsumerMatrix.cmake
```

The matrix checks:

- the SDK manifest exists and has the expected schema fields;
- the SDK DLL export snapshot can be generated and compared with the checked-in ABI baseline;
- package examples configure against only the installed SDK prefix;
- package examples build for each requested configuration;
- runtime DLLs are copied into consumer output directories;
- component selection checks still reject unintended component imports.

Generate or compare an SDK export snapshot directly:

```powershell
cmake -DSMROBOT_SDK_ABI_SDK_PREFIX:PATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final -DSMROBOT_SDK_ABI_OUTPUT:FILEPATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final/SMRobotSDKExportSnapshot.txt -DSMROBOT_SDK_ABI_BASELINE:FILEPATH=D:/program/src/RS2026_CodexDev/cmake/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_ABI_REQUIRE_BASELINE:BOOL=ON -P cmake/CheckSmRobotSdkExportSnapshot.cmake
```

The snapshot script excludes the duplicate private runtime copy
`SMRobotPlatform/bin/GLRuntime_shared_*.dll` by default. `Common::GLRuntime`
is still delivered and recorded in the SDK manifest; it is not treated as a
Platform component ABI baseline section.

Audit suspicious exported symbols for migrated DLL components:

```powershell
cmake -DSMROBOT_SDK_ABI_AUDIT_SNAPSHOT:FILEPATH=D:/program/src/RS2026_CodexDev/cmake/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_ABI_AUDIT_OUTPUT:FILEPATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final/SMRobotSDKExportAudit.migrated.txt -DSMROBOT_SDK_ABI_AUDIT_DLL_REGEX:STRING="SimulationRuntime|SensorSimulation|VisualizationSDK" -DSMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS:BOOL=ON -P cmake/AuditSmRobotSdkExportSnapshot.cmake
```

Generate a non-failing ABI inventory for render-chain components that still use
automatic symbol export:

```powershell
cmake -DSMROBOT_SDK_ABI_AUDIT_SNAPSHOT:FILEPATH=D:/program/src/RS2026_CodexDev/cmake/sdk_abi_baselines/SMRobotSDKExportSnapshot.v2.txt -DSMROBOT_SDK_ABI_AUDIT_OUTPUT:FILEPATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final/SMRobotSDKExportAudit.render-chain.txt -DSMROBOT_SDK_ABI_AUDIT_DLL_REGEX:STRING="RenderCore|SceneCore|RobotRenderBridge" -DSMROBOT_SDK_ABI_AUDIT_FAIL_ON_FINDINGS:BOOL=OFF -P cmake/AuditSmRobotSdkExportSnapshot.cmake
```

Create a release archive after manifest and ABI snapshot checks:

```powershell
cmake -DSMROBOT_SDK_RELEASE_SDK_PREFIX:PATH=D:/program/src/RS2026_CodexDev/build/sdk_e_final -DSMROBOT_SDK_RELEASE_OUTPUT_DIR:PATH=D:/program/src/RS2026_CodexDev/build/sdk_artifacts -DSMROBOT_SDK_RELEASE_ARCHIVE_NAME:STRING=SMRobotSDK-core-platform-stage-i.zip -P cmake/CreateSmRobotSdkReleaseArtifact.cmake
```

