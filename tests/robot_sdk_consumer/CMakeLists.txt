cmake_minimum_required(VERSION 3.20)

project(SMRobotGen2RobotSDKConsumer LANGUAGES CXX)

find_package(SMRobotGen2 CONFIG REQUIRED COMPONENTS RobotSDK)

add_executable(SMRobotGen2RobotSDKConsumer main.cpp)

target_compile_features(SMRobotGen2RobotSDKConsumer PRIVATE cxx_std_17)

target_link_libraries(SMRobotGen2RobotSDKConsumer PRIVATE
    SMRobotGen2::RobotSDK
)
