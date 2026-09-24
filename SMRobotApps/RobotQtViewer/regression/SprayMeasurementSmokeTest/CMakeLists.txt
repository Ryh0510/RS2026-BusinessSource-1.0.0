cmake_minimum_required(VERSION 3.20)

if(NOT TARGET MotionPlanningEditor)
    return()
endif()

set(EXAMPLE_TARGET_NAME RobotQtViewer-SprayMeasurementSmoke)
set(ROBOT_QT_VIEWER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_executable(${EXAMPLE_TARGET_NAME}
    main.cpp
    "${ROBOT_QT_VIEWER_SOURCE_DIR}/RobotQtViewerViewportServicesAdapter.cpp")
target_include_directories(${EXAMPLE_TARGET_NAME} PRIVATE "${ROBOT_QT_VIEWER_SOURCE_DIR}")
target_link_libraries(${EXAMPLE_TARGET_NAME} PRIVATE
    SMRobotApps::RobotViewerCore
    MotionPlanningEditor
    SMRobotMotionPlanning::ProjectMotionPlanning
    ${RobotQtViewer_REQUIRED_TARGETS})
target_compile_features(${EXAMPLE_TARGET_NAME} PRIVATE cxx_std_17)
set_target_properties(${EXAMPLE_TARGET_NAME} PROPERTIES
    FOLDER "Regression/SMRobotApps/RobotQtViewer"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    RELEASE_POSTFIX ${CMAKE_RELEASE_POSTFIX})
copy_runtime_dlls(${EXAMPLE_TARGET_NAME})
deploy_qt(${EXAMPLE_TARGET_NAME})
if(BuildTests)
    add_test(NAME RobotQtViewerIkPlaybackSmoke COMMAND ${EXAMPLE_TARGET_NAME}
        --ik-project "${CMAKE_SOURCE_DIR}/config/projects/ABB4600-burnner.sys.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/ik_tcp_poses.txt")
    set_tests_properties(RobotQtViewerIkPlaybackSmoke PROPERTIES
        LABELS "RobotQtViewer;Regression;IK"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${EXAMPLE_TARGET_NAME}>)
    add_test(NAME RobotQtViewerSprayMeasurementSmoke COMMAND ${EXAMPLE_TARGET_NAME})
    set_tests_properties(RobotQtViewerSprayMeasurementSmoke PROPERTIES
        LABELS "RobotQtViewer;Regression;Spray"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${EXAMPLE_TARGET_NAME}>)
endif()

if(BuildTests)
    add_test(NAME RobotQtViewerMultiIkSmoke COMMAND ${EXAMPLE_TARGET_NAME}
        --multi-ik-project "${CMAKE_SOURCE_DIR}/config/projects/ABB4600-burnner.sys.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/ik_tcp_poses.txt")
    set_tests_properties(RobotQtViewerMultiIkSmoke PROPERTIES
        LABELS "RobotQtViewer;Regression;IK"
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${EXAMPLE_TARGET_NAME}>)
endif()
