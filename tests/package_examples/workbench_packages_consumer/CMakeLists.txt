cmake_minimum_required(VERSION 3.20)

project(WorkbenchPackagesConsumer LANGUAGES C CXX)

find_package(SMRobotWorkbenchCommon CONFIG REQUIRED
    COMPONENTS
        WorkbenchCommon
        RobotQtModulesShared
        RobotQtModulesCoreWidgets
)
find_package(SMRobotWorkbenchProjectAssembly CONFIG REQUIRED
    COMPONENTS
        ProjectAssemblyWorkbench
        ProjectAssemblySceneExplorer
        ProjectAssemblyToolSetup
)
find_package(SMRobotWorkbenchCollisionConfig CONFIG REQUIRED
    COMPONENTS
        CollisionConfigWorkbench
        CollisionDetectorConfig
        CollisionLinkModelSetup
        CollisionRuntimeResults
)
find_package(SMRobotWorkbenchRobotRun CONFIG REQUIRED
    COMPONENTS
        RobotRunWorkbench
        RobotRunMotionControl
)
find_package(SMRobotWorkbenchMotionPlanning CONFIG REQUIRED COMPONENTS MotionPlanningWorkbench MotionPlanningEditor)
find_package(SMRobotWorkbenchSprayProcess CONFIG REQUIRED COMPONENTS SprayProcessWorkbench)
find_package(SMRobotWorkbenchPaintingAnalysis CONFIG REQUIRED COMPONENTS PaintingAnalysisWorkbench)
find_package(SMRobotWorkbenchDigitalTwin CONFIG REQUIRED COMPONENTS DigitalTwinWorkbench)

add_executable(WorkbenchPackagesConsumer main.cpp)

target_link_libraries(WorkbenchPackagesConsumer
    PRIVATE
        SMRobotWorkbenchCommon::WorkbenchCommon
        SMRobotWorkbenchCommon::RobotQtModulesShared
        SMRobotWorkbenchCommon::RobotQtModulesCoreWidgets
        SMRobotWorkbenchProjectAssembly::ProjectAssemblyWorkbench
        SMRobotWorkbenchProjectAssembly::ProjectAssemblySceneExplorer
        SMRobotWorkbenchProjectAssembly::ProjectAssemblyToolSetup
        SMRobotWorkbenchCollisionConfig::CollisionConfigWorkbench
        SMRobotWorkbenchCollisionConfig::CollisionDetectorConfig
        SMRobotWorkbenchCollisionConfig::CollisionLinkModelSetup
        SMRobotWorkbenchCollisionConfig::CollisionRuntimeResults
        SMRobotWorkbenchRobotRun::RobotRunWorkbench
        SMRobotWorkbenchRobotRun::RobotRunMotionControl
        SMRobotWorkbenchMotionPlanning::MotionPlanningWorkbench
        SMRobotWorkbenchMotionPlanning::MotionPlanningEditor
        SMRobotWorkbenchSprayProcess::SprayProcessWorkbench
        SMRobotWorkbenchPaintingAnalysis::PaintingAnalysisWorkbench
        SMRobotWorkbenchDigitalTwin::DigitalTwinWorkbench
)

target_compile_features(WorkbenchPackagesConsumer
    PRIVATE
        cxx_std_17
)
