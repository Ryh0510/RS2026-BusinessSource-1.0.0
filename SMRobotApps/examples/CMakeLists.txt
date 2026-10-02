cmake_minimum_required(VERSION 3.20)

rs_add_subdirectory_if_targets_available(
    RobotViewerCoreQuickStart
    SMRobotApps::RobotViewerCore
)
rs_add_subdirectory_if_targets_available(
    CustomMeshViewerQuickStart
    SMRobotApps::RobotViewerCore
)
