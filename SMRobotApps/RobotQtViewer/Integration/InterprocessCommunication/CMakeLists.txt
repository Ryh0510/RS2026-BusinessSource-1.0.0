cmake_minimum_required(VERSION 3.20)
find_package(Qt5 5.15 CONFIG REQUIRED COMPONENTS Network)

# Application bridge compiled in its host to avoid a RobotViewerCore dependency cycle.
add_library(RobotQtViewerInterprocessCommunication INTERFACE)
add_library(RobotQtViewer::InterprocessCommunication ALIAS RobotQtViewerInterprocessCommunication)

target_include_directories(RobotQtViewerInterprocessCommunication INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
target_sources(RobotQtViewerInterprocessCommunication INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/RwsPythonBridge.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/RwsPythonBridge.cpp>
)
target_compile_features(RobotQtViewerInterprocessCommunication INTERFACE cxx_std_17)
target_link_libraries(RobotQtViewerInterprocessCommunication INTERFACE Qt5::Network SMRobotMotionPlanning::ProjectMotionPlanning)
