#################################################################################
#   Local Windows dependency configuration for this workstation.
#################################################################################

get_filename_component(this_cmake_file "${CMAKE_CURRENT_LIST_FILE}" ABSOLUTE)
message(STATUS "\n------------------ Find packages in ${this_cmake_file} ------------------")

set(SAVE_MODULE_PATH ${CMAKE_MODULE_PATH})
include(CMakePrintHelpers)

get_filename_component(_rs2026_default_local_prebuild "${PROJECT_SOURCE_DIR}/../prebuild" ABSOLUTE)
set(LOCAL_PREBUILD_ROOT "${_rs2026_default_local_prebuild}" CACHE PATH
    "Local prebuilt third-party dependency root for this workstation.")
get_filename_component(LOCAL_PREBUILD_ROOT "${LOCAL_PREBUILD_ROOT}" ABSOLUTE)
set(PREBUILD_DIR "${LOCAL_PREBUILD_ROOT}" CACHE PATH
    "Local prebuilt third-party dependency root for this workstation." FORCE)

get_filename_component(BUILDINLIB_DIR "${SMROBOT_THIRDPARTY_ROOT}" ABSOLUTE)
if(NOT EXISTS "${BUILDINLIB_DIR}/Windows/Findglfw_3rdParty.cmake")
    get_filename_component(_rs2026_source_thirdparty
        "${PROJECT_SOURCE_DIR}/thirdparty"
        ABSOLUTE)
    if(EXISTS "${_rs2026_source_thirdparty}/Windows/Findglfw_3rdParty.cmake")
        set(BUILDINLIB_DIR "${_rs2026_source_thirdparty}")
        set(SMROBOT_THIRDPARTY_ROOT "${BUILDINLIB_DIR}" CACHE PATH
            "Third-party dependency root for this workstation."
            FORCE)
        set(SMROBOT_PREBUILT_THIRDPARTY_ROOT "${BUILDINLIB_DIR}" CACHE PATH
            "Prebuilt third-party dependency root for this workstation."
            FORCE)
    endif()
endif()
message(STATUS "   -- Build in library Directory: ${BUILDINLIB_DIR}")
message(STATUS "   -- Local prebuild Directory: ${LOCAL_PREBUILD_ROOT}")

function(_rs2026_local_assert_dir DIR DESCRIPTION)
    if(NOT IS_DIRECTORY "${DIR}")
        message(FATAL_ERROR "${DESCRIPTION} does not exist: ${DIR}")
    endif()
endfunction()

function(_rs2026_local_assert_target PROVIDER)
    foreach(TARGET_NAME IN LISTS ARGN)
        if(NOT TARGET ${TARGET_NAME})
            message(FATAL_ERROR "${PROVIDER} target is missing: ${TARGET_NAME}")
        endif()
    endforeach()
endfunction()

function(_rs2026_local_fix_shared_runtime TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        return()
    endif()

    get_target_property(_target_type ${TARGET_NAME} TYPE)
    if(NOT _target_type STREQUAL "SHARED_LIBRARY")
        return()
    endif()

    foreach(_config DEBUG RELEASE)
        get_target_property(_runtime_file ${TARGET_NAME} IMPORTED_LOCATION_${_config})
        if(NOT _runtime_file)
            get_target_property(_implib_file ${TARGET_NAME} IMPORTED_IMPLIB_${_config})
            if(_implib_file)
                get_filename_component(_implib_dir "${_implib_file}" DIRECTORY)
                get_filename_component(_implib_name "${_implib_file}" NAME_WE)
                set(_candidate_runtime "${_implib_dir}/${_implib_name}.dll")
                if(EXISTS "${_candidate_runtime}")
                    set_target_properties(${TARGET_NAME} PROPERTIES
                        IMPORTED_LOCATION_${_config} "${_candidate_runtime}")
                    message(STATUS
                        "Fixed runtime for ${TARGET_NAME} ${_config}: ${_candidate_runtime}")
                endif()
            endif()
        endif()
    endforeach()
endfunction()

_rs2026_local_assert_dir("${LOCAL_PREBUILD_ROOT}" "Local prebuild root")

# Header-only packages bundled with the source tree.
set(CMAKE_MODULE_PATH "${BUILDINLIB_DIR}")
include(Findglm_3rdParty)
include(FindEigen_3rdParty)
include(Findnlohmann_json_3rdParty)
_rs2026_local_assert_target(glm glm::glm)
_rs2026_local_assert_target(Eigen3 Eigen3::Eigen)
_rs2026_local_assert_target(nlohmann_json nlohmann_json::nlohmann_json)

# Windows dependencies bundled with the source tree.
set(CMAKE_MODULE_PATH "${BUILDINLIB_DIR}/Windows")
include(Findglfw_3rdParty)
include(Findfreeglut_3rdParty)
include(Findpinocchio_3rdParty)
include(FindCoACD_3rdParty)
_rs2026_local_assert_target(glfw glfw::glfw)
_rs2026_local_assert_target(FreeGLUT FreeGLUT::freeglut)
_rs2026_local_assert_target(pinocchio pinocchio::pinocchio)
_rs2026_local_assert_target(CoACD CoACD::coacd)

# Boost 1.78.0 from the workstation prebuild directory.
set(_rs2026_boost_root "${LOCAL_PREBUILD_ROOT}/Boost-1.78.0/boost_1_78_0")
set(BOOST_ROOT "${_rs2026_boost_root}" CACHE PATH "Boost 1.78.0 root." FORCE)
set(Boost_DIR "${_rs2026_boost_root}/lib64-msvc-14.2/cmake/Boost-1.78.0" CACHE PATH
    "Boost 1.78.0 CMake package directory." FORCE)
_rs2026_local_assert_dir("${Boost_DIR}" "Boost CMake package directory")

set(Boost_USE_DEBUG_LIBS ON)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_NO_SYSTEM_PATHS ON)

set(_rs2026_boost_dynamic_components log log_setup)
set(_rs2026_boost_static_components
    headers
    atomic
    chrono
    date_time
    exception
    filesystem
    iostreams
    json
    locale
    program_options
    random
    regex
    serialization
    system
    thread
    timer
)

set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost 1.78.0 EXACT CONFIG REQUIRED
    COMPONENTS ${_rs2026_boost_dynamic_components}
    PATHS "${Boost_DIR}"
    NO_DEFAULT_PATH)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.78.0 EXACT CONFIG REQUIRED
    COMPONENTS ${_rs2026_boost_static_components}
    PATHS "${Boost_DIR}"
    NO_DEFAULT_PATH)

foreach(_boost_component IN LISTS _rs2026_boost_dynamic_components _rs2026_boost_static_components)
    _rs2026_local_assert_target(Boost "Boost::${_boost_component}")
    _rs2026_local_fix_shared_runtime("Boost::${_boost_component}")
endforeach()
target_compile_definitions(Boost::log INTERFACE BOOST_LOG_DYN_LINK)
target_compile_definitions(Boost::log_setup INTERFACE BOOST_LOG_DYN_LINK)
message(STATUS "Found Boost ${Boost_VERSION} at ${Boost_DIR}")

# Assimp 6.0.2 from the workstation prebuild directory.
set(assimp_DIR "${LOCAL_PREBUILD_ROOT}/assimp-6.0.2/Assimp6.0.2/lib/cmake/assimp-6.0" CACHE PATH
    "Assimp 6.0.2 CMake package directory." FORCE)
_rs2026_local_assert_dir("${assimp_DIR}" "Assimp CMake package directory")
find_package(assimp CONFIG REQUIRED
    PATHS "${assimp_DIR}"
    NO_DEFAULT_PATH)
_rs2026_local_assert_target(assimp assimp::assimp)
message(STATUS "Found assimp ${assimp_VERSION} at ${assimp_DIR}")

# FCL static package from the workstation prebuild directory.
set(CMAKE_MODULE_PATH "${LOCAL_PREBUILD_ROOT}/fcl-0.7.0-static/fcl_static_x64")
include(CMake_FindFCL)
_rs2026_local_assert_target(FCL fcl::fcl ccd::ccd octomap::octomap)

find_package(Threads REQUIRED)

# OMPL 2.0.1 from the workstation prebuild directory.
set(SMROBOT_OMPL_INSTALL_DIR "${LOCAL_PREBUILD_ROOT}/ompl-2.0.1/ompl-2.0.1" CACHE PATH
    "OMPL 2.0.1 install prefix." FORCE)
set(ompl_DIR "${SMROBOT_OMPL_INSTALL_DIR}/share/ompl/cmake" CACHE PATH
    "OMPL 2.0.1 CMake package directory." FORCE)
_rs2026_local_assert_dir("${ompl_DIR}" "OMPL CMake package directory")
find_package(ompl 2.0.1 EXACT CONFIG REQUIRED
    PATHS "${ompl_DIR}"
    NO_DEFAULT_PATH)
_rs2026_local_assert_target(OMPL ompl::ompl)
set_property(TARGET ompl::ompl PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set_property(TARGET ompl::ompl PROPERTY MAP_IMPORTED_CONFIG_MINSIZEREL Release)
message(STATUS "Found OMPL ${OMPL_VERSION} at ${ompl_DIR}")

# Qt 5.15.5 from the workstation prebuild directory.
set(_rs2026_qt_root "${LOCAL_PREBUILD_ROOT}/Qt5155_x64/Qt5155_x64")
set(Qt5_DIR "${_rs2026_qt_root}/lib/cmake/Qt5" CACHE PATH
    "Qt 5.15.5 CMake package directory." FORCE)
_rs2026_local_assert_dir("${Qt5_DIR}" "Qt5 CMake package directory")
set(Qt_Components Core Gui Widgets OpenGL)
find_package(Qt5 CONFIG REQUIRED
    COMPONENTS ${Qt_Components}
    PATHS "${Qt5_DIR}"
    NO_DEFAULT_PATH)
foreach(_qt_component IN LISTS Qt_Components)
    _rs2026_local_assert_target(Qt5 "Qt5::${_qt_component}")
endforeach()
_rs2026_local_assert_target(Qt5 Qt5::qmake)
message(STATUS "Found Qt ${Qt5_VERSION} at ${Qt5_DIR}")

find_package(OpenGL REQUIRED)
if(TARGET OpenGL::GL)
    message(STATUS "Target OpenGL::GL is FOUND!")
endif()

set(OSQP_SOURCE_DIR "${LOCAL_PREBUILD_ROOT}/osqp-0.6.3" CACHE PATH
    "OSQP 0.6.3 source directory." FORCE)
_rs2026_local_assert_dir("${OSQP_SOURCE_DIR}" "OSQP source directory")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
    "Compatibility floor for older third-party CMake projects." FORCE)
add_subdirectory("${OSQP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/prebuild/osqp-0.6.3" EXCLUDE_FROM_ALL)
_rs2026_local_assert_target(osqpstatic)
message(STATUS "Found OSQP source tree at ${OSQP_SOURCE_DIR}")

set(CMAKE_MODULE_PATH ${SAVE_MODULE_PATH})
message(STATUS "\n------------------ End of finding packages in ${this_cmake_file} ------------------")
