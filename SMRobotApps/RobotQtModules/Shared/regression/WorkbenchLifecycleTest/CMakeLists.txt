cmake_minimum_required(VERSION 3.20)

set(TEST_TARGET_NAME RobotQtModulesShared-WorkbenchLifecycleTest)

add_executable(${TEST_TARGET_NAME} main.cpp)
target_link_libraries(${TEST_TARGET_NAME}
    PRIVATE
        SMRobotWorkbenchCommon::RobotQtModulesShared
)
target_compile_features(${TEST_TARGET_NAME} PRIVATE cxx_std_17)
set_target_properties(${TEST_TARGET_NAME} PROPERTIES
    FOLDER tests/SMRobotWorkbenchCommon
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)

copy_runtime_dlls(${TEST_TARGET_NAME})
if(COMMAND smrobot_deploy_runtime_licenses)
    smrobot_deploy_runtime_licenses(${TEST_TARGET_NAME})
endif()

add_test(
    NAME RobotQtModulesShared-WorkbenchLifecycleTest
    COMMAND $<TARGET_FILE:${TEST_TARGET_NAME}>
)
set_tests_properties(RobotQtModulesShared-WorkbenchLifecycleTest PROPERTIES
    LABELS "SMRobotWorkbenchCommon;RobotQtViewer;Lifecycle"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_TARGET_NAME}>
)
