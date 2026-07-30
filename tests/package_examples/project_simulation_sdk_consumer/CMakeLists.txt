cmake_minimum_required(VERSION 3.20)

project(ProjectSimulationSdkConsumer LANGUAGES CXX)

find_package(SMRobotPlatform CONFIG REQUIRED COMPONENTS ProjectSimulationSDK)

add_executable(ProjectSimulationSdkConsumer main.cpp)
target_compile_features(ProjectSimulationSdkConsumer PRIVATE cxx_std_17)
target_link_libraries(ProjectSimulationSdkConsumer PRIVATE SMRobotPlatform::ProjectSimulationSDK)

if(WIN32)
    add_custom_command(TARGET ProjectSimulationSdkConsumer POST_BUILD
        COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:ProjectSimulationSdkConsumer>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:ProjectSimulationSdkConsumer>;$<TARGET_FILE_DIR:ProjectSimulationSdkConsumer>>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotPlatform::ProjectSimulationSDK>" "$<TARGET_FILE_DIR:ProjectSimulationSdkConsumer>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying ProjectSimulationSdkConsumer runtime DLLs"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME ProjectSimulationSDK.AbiSmoke
        COMMAND ProjectSimulationSdkConsumer
    )
endif()
