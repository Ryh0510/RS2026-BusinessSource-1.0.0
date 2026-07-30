cmake_minimum_required(VERSION 3.20)

project(SMRobotCoreComponentsSmoke LANGUAGES CXX)

include(CTest)

find_package(SMRobotCore CONFIG REQUIRED COMPONENTS
    RobotCore
    Kinematics
    Collision
    RobotTrajectoryCore
    RobotRuntime
    RobotInstance
)

add_executable(SMRobotCoreComponentsSmoke main.cpp)
target_compile_features(SMRobotCoreComponentsSmoke PRIVATE cxx_std_17)
target_link_libraries(SMRobotCoreComponentsSmoke PRIVATE
    SMRobotCore::RobotCore
    SMRobotCore::Kinematics
    SMRobotCore::Collision
    SMRobotCore::RobotTrajectoryCore
    SMRobotCore::RobotRuntime
    SMRobotCore::RobotInstance
)

add_custom_command(TARGET SMRobotCoreComponentsSmoke POST_BUILD
    COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:SMRobotCoreComponentsSmoke>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:SMRobotCoreComponentsSmoke>;$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::RobotCore>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::Kinematics>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::Collision>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::RobotTrajectoryCore>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::RobotRuntime>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:SMRobotCore::RobotInstance>" "$<TARGET_FILE_DIR:SMRobotCoreComponentsSmoke>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying SMRobotCoreComponentsSmoke runtime DLLs"
)

if(BUILD_TESTING)
    add_test(
        NAME SMRobotCore.RobotCoreSmoke
        COMMAND SMRobotCoreComponentsSmoke RobotCore
    )
    add_test(
        NAME SMRobotCore.KinematicsSmoke
        COMMAND SMRobotCoreComponentsSmoke Kinematics
    )
    add_test(
        NAME SMRobotCore.CollisionSmoke
        COMMAND SMRobotCoreComponentsSmoke Collision
    )
    add_test(
        NAME SMRobotCore.RobotTrajectoryCoreSmoke
        COMMAND SMRobotCoreComponentsSmoke RobotTrajectoryCore
    )
    add_test(
        NAME SMRobotCore.RobotRuntimeSmoke
        COMMAND SMRobotCoreComponentsSmoke RobotRuntime
    )
    add_test(
        NAME SMRobotCore.RobotInstanceSmoke
        COMMAND SMRobotCoreComponentsSmoke RobotInstance
    )
endif()
