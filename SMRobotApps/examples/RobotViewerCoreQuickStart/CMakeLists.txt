cmake_minimum_required(VERSION 3.20)

set(_smrobot_quickstart_standalone OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(RobotViewerCoreQuickStart LANGUAGES CXX)
    set(_smrobot_quickstart_standalone ON)
    find_package(SMRobotApps CONFIG REQUIRED COMPONENTS RobotViewerCore)
endif()

add_executable(RobotViewerCoreQuickStart main.cpp)
target_compile_features(RobotViewerCoreQuickStart PRIVATE cxx_std_17)
target_link_libraries(RobotViewerCoreQuickStart PRIVATE SMRobotApps::RobotViewerCore)

if(NOT _smrobot_quickstart_standalone)
    set_target_properties(RobotViewerCoreQuickStart PROPERTIES
        FOLDER "Examples/SMRobotApps")
    copy_runtime_dlls(RobotViewerCoreQuickStart)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME RobotViewerCoreQuickStart COMMAND RobotViewerCoreQuickStart)
endif()
