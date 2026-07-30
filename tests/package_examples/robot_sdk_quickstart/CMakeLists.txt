cmake_minimum_required(VERSION 3.20)

project(SMRobotGen2SdkQuickStart LANGUAGES CXX)

find_package(SMRobotGen2 CONFIG REQUIRED COMPONENTS RobotSDK)

add_executable(SMRobotGen2SdkQuickStart main.cpp)
target_compile_features(SMRobotGen2SdkQuickStart PRIVATE cxx_std_17)
target_link_libraries(SMRobotGen2SdkQuickStart PRIVATE SMRobotGen2::RobotSDK)

