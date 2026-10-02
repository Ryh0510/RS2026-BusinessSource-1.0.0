cmake_minimum_required(VERSION 3.20)

rs_example_category_enabled(_build_example Regression ${PACKAGE_NAME})
if(NOT _build_example)
    return()
endif()

set(EXAMPLE_TARGET_NAME RobotViewerCore-CustomMeshViewportSmokeTest)

add_executable(${EXAMPLE_TARGET_NAME} main.cpp)
target_link_libraries(${EXAMPLE_TARGET_NAME}
    PRIVATE
        SMRobotApps::RobotViewerCore
        Qt5::Widgets
)
target_compile_features(${EXAMPLE_TARGET_NAME} PRIVATE cxx_std_17)
set_target_properties(${EXAMPLE_TARGET_NAME}
    PROPERTIES
        FOLDER "Regression/SMRobotApps/RobotViewerCore"
        DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
        RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX}
)

copy_runtime_dlls(${EXAMPLE_TARGET_NAME})
deploy_qt(${EXAMPLE_TARGET_NAME})

if(BuildTests)
    add_test(
        NAME SMRobotGen2RobotViewerCoreCustomMeshViewportSmokeTest
        COMMAND ${EXAMPLE_TARGET_NAME}
    )
    set_tests_properties(
        SMRobotGen2RobotViewerCoreCustomMeshViewportSmokeTest
        PROPERTIES
            LABELS "SMRobotGen2;RobotViewerCore;Visualization;OpenGL;Regression"
            WORKING_DIRECTORY $<TARGET_FILE_DIR:${EXAMPLE_TARGET_NAME}>
            TIMEOUT 30
    )
endif()
