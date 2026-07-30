cmake_minimum_required(VERSION 3.20)

project(CDFPlanningDemos LANGUAGES CXX)

option(CDF_BUILD_GUI "Build the Qt CDF planning demonstration app." ON)
option(CDF_BUILD_ALGORITHM_SMOKE "Build the headless CDF algorithm smoke test." ON)

add_subdirectory(Algorithms)

if(CDF_BUILD_GUI)
    add_subdirectory(Gui)
endif()
