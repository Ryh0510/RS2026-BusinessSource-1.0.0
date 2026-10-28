set(_target RobotQtViewer-ModelEnvironmentTest)
add_executable(${_target} main.cpp ../../RobotQtViewerEnvironmentSource.cpp ../../RobotQtViewerEnvironmentController.cpp)
target_link_libraries(${_target} PRIVATE SMRobotApps::RobotViewerCore RobotQtModulesShared Qt5::Widgets)
target_compile_features(${_target} PRIVATE cxx_std_17)
set_target_properties(${_target} PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX} RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX})
copy_runtime_dlls(${_target})
deploy_qt(${_target})
add_test(NAME RobotQtViewerModelEnvironment COMMAND ${_target}
    "${PROJECT_SOURCE_DIR}/config/environments/factory-detailed.environment.json"
    "${PROJECT_SOURCE_DIR}/data/environments/factory-detailed/runtime")
set_tests_properties(RobotQtViewerModelEnvironment PROPERTIES
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${_target}> TIMEOUT 240
    LABELS "RobotQtViewer;ModelEnvironment;OpenGL;Regression")
