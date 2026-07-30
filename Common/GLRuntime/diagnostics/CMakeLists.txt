##########################################################################
#	CMake requirement
##########################################################################
cmake_minimum_required( VERSION 3.20 )


#   1. Get all example names
set( ExcludeTargets CMakeLists.txt )
collect_subdirs(
    ${CMAKE_CURRENT_LIST_DIR}
    AllExampleNames
    EXCLUDE ${ExcludeTargets}
)

set( ExampleSet ${AllExampleNames} )

message( STATUS "-------------------------------------------------------")
message( STATUS "All Examples in Package ${PACKAGE_NAME}: ${ExampleSet}" )
message( STATUS "-------------------------------------------------------\n\n")


#   2. Add all example targets
foreach( EXAMPLE_NAME ${ExampleSet} )
    add_subdirectory( ${EXAMPLE_NAME} )
endforeach()
