cmake_minimum_required(VERSION 3.20)

include(${PROJECT_SOURCE_DIR}/cmake/SMRobotWorkbenchPlugin.cmake)

set(PLUGIN_TARGET RobotQtViewer-ExampleInspectionWorkbenchPlugin)
add_library(${PLUGIN_TARGET} MODULE
    ExampleInspectionWorkbenchPlugin.cpp
    ExampleInspectionWorkbenchPlugin.h
)
target_link_libraries(${PLUGIN_TARGET}
    PRIVATE
        SMRobotWorkbenchCommon::RobotQtModulesShared
        Qt5::Widgets
)
target_compile_features(${PLUGIN_TARGET} PRIVATE cxx_std_17)
set_target_properties(${PLUGIN_TARGET} PROPERTIES
    FOLDER tests/SMRobotApps/RobotQtViewer
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)

smrobot_stage_workbench_plugin(
    TARGET ${PLUGIN_TARGET}
    MANIFEST_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/example-inspection.workbench.json.in
    PACKAGE_ID smrobot.example.inspection
    INSTALL_DESTINATION SMRobotApps/tests/workbenches
)

set(TEST_TARGET RobotQtViewer-WorkbenchPluginTest)
add_executable(${TEST_TARGET} main.cpp)
target_link_libraries(${TEST_TARGET}
    PRIVATE
        SMRobotWorkbenchCommon::RobotQtModulesShared
        Qt5::Widgets
)
target_compile_features(${TEST_TARGET} PRIVATE cxx_std_17)
add_dependencies(${TEST_TARGET} ${PLUGIN_TARGET})
set_target_properties(${TEST_TARGET} PROPERTIES
    FOLDER tests/SMRobotApps/RobotQtViewer
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)
copy_runtime_dlls(${TEST_TARGET})

add_test(
    NAME RobotQtViewer-WorkbenchPluginTest
    COMMAND $<TARGET_FILE:${TEST_TARGET}>
        ${${PLUGIN_TARGET}_WORKBENCH_STAGING_DIRECTORY}
)
set_tests_properties(RobotQtViewer-WorkbenchPluginTest PROPERTIES
    LABELS "SMRobotApps;RobotQtViewer;WorkbenchPlugin"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_TARGET}>
)
