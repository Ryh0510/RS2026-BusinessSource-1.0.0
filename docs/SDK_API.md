# SMRobotCore RobotSDK API

## Scope

`RobotSDK` is the public SDK facade for robot loading, model inspection,
runtime joint updates, base/link transforms, and headless collision checks.
SDK users should include headers from `RobotSDK/` and link only
`SMRobotCore::RobotSDK`.

Internal targets such as `RobotCore`, `RobotIO`, `RobotInstance`,
`Kinematics`, and `Collision` are implementation details of the SDK facade.
Do not include their headers in external applications.

## Lifetime

Create and destroy the SDK through the exported C functions:

```cpp
smrobotgen2::sdk::IRobotSdk* sdk = createRobotSdk();
destroyRobotSdk(sdk);
```

Objects created by `IRobotSdk` must be destroyed by the matching destroy
method on the same `IRobotSdk` instance:

```cpp
auto* loader = sdk->createRobotLoader();
sdk->destroyRobotLoader(loader);
```

Do not delete SDK interface pointers directly across the DLL boundary.

## Versioning

Use these functions to check the package at runtime:

```cpp
const char* version = getSdkVersion();
int abi = getSdkAbiVersion();
```

The current ABI value is also available through `IRobotSdk::abiVersion()`.
When a virtual interface changes, the ABI value must be increased.

## Loading Robots

`IRobotLoader::load` accepts `RobotSourceType::Urdf` or
`RobotSourceType::Simscape` and returns `LoadRobotResult`.

On success, `LoadRobotResult::model` is non-null. On failure, inspect
`LoadRobotResult::status.message` or `IRobotLoader::lastErrorMessage()`.

## Model Queries

`IRobotModel` exposes stable read-only model metadata:

- `name()`
- `rootLink()`
- `linkCount()`
- `jointCount()`
- `dof()`
- `linkName(index)`
- `jointName(index)`
- `jointDofIndex(index)`
- `linkIndex(name)`
- `jointIndex(name)`

Invalid names or indexes return an empty string or `-1`.

## Runtime Instance

Create an instance from a loaded model:

```cpp
auto* instance = sdk->createRobotInstance(model, "robot_1");
```

Use `setJoint`, `setJoints`, and `setBaseTransform`, then call `update()`.
Use `linkTransform(linkName, &transform)` to query the updated link pose.

`Transform::values` stores a 4x4 row-major matrix.

## Collision World

`ICollisionWorld` supports:

- robot instances
- environment boxes, spheres, and cylinders
- environment transforms
- robot-robot pair enable/disable
- robot-environment pair enable/disable
- collision check, first contact, contact list, and distance query

After changing robots or environment objects, call `update()` before
querying collision or distance.

## Error Handling

Most mutating functions return `Result`.

```cpp
smrobotgen2::sdk::Result result = instance->update();
if (smrobotgen2::sdk::failed(result))
{
    std::cerr << result.message << std::endl;
}
```

The SDK does not throw exceptions through the public interface.
