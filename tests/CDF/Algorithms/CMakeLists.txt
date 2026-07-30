cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME CDFAlgorithms)

add_library(${TARGET_NAME} STATIC
    include/CDFAlgorithms/CdfCollisionOracle.h
    include/CDFAlgorithms/CdfDocument.h
    include/CDFAlgorithms/CdfPlanningService.h
    include/CDFAlgorithms/CdfTypes.h
    src/CdfCollisionOracle.cpp
    src/CdfDocument.cpp
    src/CdfPlanningService.cpp
)

add_library(CDF::Algorithms ALIAS ${TARGET_NAME})

target_include_directories(${TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(${TARGET_NAME}
    PUBLIC
        cxx_std_17
)

set_target_properties(${TARGET_NAME}
    PROPERTIES
        FOLDER "tests/CDF/Algorithms"
)

if(CDF_BUILD_ALGORITHM_SMOKE)
    add_executable(CDFAlgorithmSmoke
        smoke/CdfAlgorithmSmoke.cpp
    )

    target_link_libraries(CDFAlgorithmSmoke
        PRIVATE
            CDF::Algorithms
    )

    target_compile_features(CDFAlgorithmSmoke
        PRIVATE
            cxx_std_17
    )

    set_target_properties(CDFAlgorithmSmoke
        PROPERTIES
            FOLDER "tests/CDF/Algorithms"
    )
endif()
