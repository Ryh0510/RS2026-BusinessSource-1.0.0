cmake_minimum_required(VERSION 3.20)
project(CommonUtilityQuickStart LANGUAGES CXX)

find_package(Common CONFIG REQUIRED COMPONENTS Utility)
add_executable(CommonUtilityQuickStart main.cpp)
target_compile_features(CommonUtilityQuickStart PRIVATE cxx_std_17)
target_link_libraries(CommonUtilityQuickStart PRIVATE Common::Utility)
