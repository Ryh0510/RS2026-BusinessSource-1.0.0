cmake_minimum_required(VERSION 3.20)

project(SMRobotCoreRobotIOUrdfConsumer LANGUAGES CXX)

find_package(SMRobotCore CONFIG REQUIRED COMPONENTS RobotIO RobotCore)

add_executable(SMRobotCoreRobotIOUrdfConsumer main.cpp)
target_compile_features(SMRobotCoreRobotIOUrdfConsumer PRIVATE cxx_std_17)
target_link_libraries(SMRobotCoreRobotIOUrdfConsumer PRIVATE
    SMRobotCore::RobotIO
    SMRobotCore::RobotCore
)

if(WIN32)
    add_custom_command(TARGET SMRobotCoreRobotIOUrdfConsumer POST_BUILD
        COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:SMRobotCoreRobotIOUrdfConsumer>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:SMRobotCoreRobotIOUrdfConsumer>;$<TARGET_FILE_DIR:SMRobotCoreRobotIOUrdfConsumer>>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::RobotIO>" "$<TARGET_FILE_DIR:SMRobotCoreRobotIOUrdfConsumer>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying SMRobotCoreRobotIOUrdfConsumer runtime DLLs"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    set(SMROBOT_PACKAGE_EXAMPLE_URDF
        "${CMAKE_CURRENT_LIST_DIR}/../../../data/URDF/ur_description/urdf/ur5.urdf"
        CACHE FILEPATH
        "URDF file used by the RobotIO package consumer smoke test."
    )
    add_test(
        NAME SMRobotCore.RobotIOUrdfLoad
        COMMAND SMRobotCoreRobotIOUrdfConsumer "${SMROBOT_PACKAGE_EXAMPLE_URDF}"
    )
endif()
