include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/RS2026CacheOptionNames.cmake")
set(_RS2026_PROVIDER_HELPER_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(_rs_demand_default ON)
if(DEFINED RS2026_4090_ENABLE_DEMAND_LOADING)
    set(_rs_demand_default "${RS2026_4090_ENABLE_DEMAND_LOADING}")
endif()
rs2026_named_cache_option(Dependency_DemandLoading RS2026_ENABLE_DEMAND_LOADING
    "Load only dependency providers required by the selected build" ${_rs_demand_default})
set(_rs_report_default ON)
if(DEFINED RS2026_4090_DEPENDENCY_REPORT)
    set(_rs_report_default "${RS2026_4090_DEPENDENCY_REPORT}")
endif()
rs2026_named_cache_option(Dependency_Report RS2026_DEPENDENCY_REPORT
    "Report dependency-provider decisions" ${_rs_report_default})
# The original profile flags remain accepted as compatibility inputs.
if(DEFINED RS2026_4090_ENABLE_DEMAND_LOADING AND
   NOT "${RS2026_4090_ENABLE_DEMAND_LOADING}" STREQUAL "${RS2026_ENABLE_DEMAND_LOADING}")
    message(WARNING "Global RS2026_ENABLE_DEMAND_LOADING overrides the legacy 4090 option")
endif()
set(RS2026_4090_ENABLE_DEMAND_LOADING "${RS2026_ENABLE_DEMAND_LOADING}")
set(RS2026_4090_DEPENDENCY_REPORT "${RS2026_DEPENDENCY_REPORT}")
set(RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR "" CACHE PATH
    "Optional local finder directory; matching finder files override profile defaults")

function(rs_dependency_provider_required out_load provider_name)
    set(_provider_packages ${ARGN})
    set(_matched_packages)
    foreach(_package IN LISTS _provider_packages)
        rs_project_requires_package("${_package}" _package_required)
        if(_package_required)
            list(APPEND _matched_packages "${_package}")
        endif()
    endforeach()

    if(_matched_packages)
        set(_demand_action LOAD)
        set(_reason "required packages: ${_matched_packages}")
    elseif(NOT DEFINED RS2026_DEPENDENCY_SNAPSHOT_COMPLETE OR
           NOT RS2026_DEPENDENCY_SNAPSHOT_COMPLETE)
        set(_demand_action LOAD)
        set(_reason "dependency snapshot is incomplete; conservative fallback")
    else()
        set(_demand_action SKIP)
        set(_reason "not required by the current build graph")
    endif()

    if(RS2026_ENABLE_DEMAND_LOADING)
        set(_actual_action "${_demand_action}")
    else()
        set(_actual_action LOAD)
    endif()

    if(RS2026_DEPENDENCY_REPORT)
        message(STATUS
            "[Dependency ${RS2026_SELECTED_PROFILE}] ${provider_name}: demand=${_demand_action}, "
            "actual=${_actual_action}, reason=${_reason}")
    endif()

    if(_actual_action STREQUAL "LOAD")
        set(${out_load} ON PARENT_SCOPE)
    else()
        set(${out_load} OFF PARENT_SCOPE)
    endif()
endfunction()

function(rs_assert_dependency_provider_targets provider_name)
    if(RS2026_ENABLE_DEMAND_LOADING)
        rs_assert_required_targets("${provider_name}" ${ARGN})
    endif()
endfunction()

function(rs_prebuilt_component_required out_required package component)
    set(_required OFF)
    if(DEFINED UsingPrebuilt_${package} AND UsingPrebuilt_${package})
        rs_project_requires_component("${package}" "${component}" _required)
    endif()
    set(${out_required} "${_required}" PARENT_SCOPE)
endfunction()

function(rs_force_dependency_provider out_load provider_name reason)
    set(${out_load} ON PARENT_SCOPE)
    if(RS2026_DEPENDENCY_REPORT)
        message(STATUS
            "[Dependency ${RS2026_SELECTED_PROFILE}] ${provider_name}: force actual=LOAD, reason=${reason}")
    endif()
endfunction()


macro(rs_include_dependency_provider finder)
    set(_rs_provider_file "")
    if(RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR AND
       EXISTS "${RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR}/${finder}.cmake")
        set(_rs_provider_file "${RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR}/${finder}.cmake")
    else()
        unset(_rs_provider_search_file CACHE)
        find_file(_rs_provider_search_file "${finder}.cmake"
            PATHS ${CMAKE_MODULE_PATH} "${_RS2026_PROVIDER_HELPER_DIR}"
                "${PROJECT_SOURCE_DIR}/cmake" NO_DEFAULT_PATH)
        set(_rs_provider_file "${_rs_provider_search_file}")
        unset(_rs_provider_search_file CACHE)
    endif()
    if(NOT _rs_provider_file OR NOT EXISTS "${_rs_provider_file}")
        message(FATAL_ERROR "[Dependency ${RS2026_SELECTED_PROFILE}] Missing requested finder "
            "${finder}.cmake in ${CMAKE_MODULE_PATH}. Set PREBUILD_DIR, "
            "SMROBOT_THIRDPARTY_ROOT or RS2026_DEPENDENCY_PROVIDER_OVERRIDE_DIR for this machine.")
    endif()
    set_property(GLOBAL APPEND PROPERTY RS2026_LOADED_PROVIDER_FILES "${_rs_provider_file}")
    include("${_rs_provider_file}")
endmacro()

function(rs_assert_requested_provider_targets)
    if(NOT RS2026_ENABLE_DEMAND_LOADING)
        return()
    endif()
    foreach(_package IN LISTS ARGN)
        foreach(_component IN LISTS RS2026_REQUIRED_COMPONENTS_${_package})
            set(_required "${_package}::${_component}")
            if(NOT TARGET "${_required}" AND TARGET "${_component}")
                add_library("${_required}" ALIAS "${_component}")
            endif()
            rs_assert_required_targets("${RS2026_SELECTED_PROFILE}" "${_required}")
        endforeach()
    endforeach()
endfunction()

function(rs_optional_glfw_required out)
    set(_required OFF)
    foreach(_option DevTools_Diagnostics DevTools_FeatureProbes
                    DevTools_PackageConsumers BuildTutorials)
        if(${_option})
            set(_required ON)
        endif()
    endforeach()
    foreach(_package IN LISTS AllBuildPackages)
        if(NOT BuildPackage_${_package} AND NOT UsingPrebuilt_${_package})
            continue()
        endif()
        if(DEFINED BuildExample_${_package} AND BuildExample_${_package})
            set(_required ON)
        endif()
        foreach(_option DevTools_Diagnostics DevTools_FeatureProbes
                        DevTools_PackageConsumers BuildTutorials)
            if(${_option}_${_package})
                set(_required ON)
            endif()
        endforeach()
    endforeach()
    set(${out} "${_required}" PARENT_SCOPE)
endfunction()

function(rs_optional_freeglut_required out)
    set(_required OFF)
    if((BuildPackage_Common OR UsingPrebuilt_Common) AND
       (DevTools_Diagnostics OR DevTools_Diagnostics_Common OR
        DevTools_FeatureProbes OR DevTools_FeatureProbes_Common))
        set(_required ON)
    endif()
    if((BuildPackage_SMRobotPlatform OR UsingPrebuilt_SMRobotPlatform) AND
       (DevTools_FeatureProbes OR DevTools_FeatureProbes_SMRobotPlatform))
        set(_required ON)
    endif()
    set(${out} "${_required}" PARENT_SCOPE)
endfunction()

function(rs_optional_boost_program_options_required out)
    set(_required OFF)
    if((BuildPackage_SMRobotPlatform OR UsingPrebuilt_SMRobotPlatform) AND
       (DevTools_FeatureProbes OR DevTools_FeatureProbes_SMRobotPlatform))
        set(_required ON)
    endif()
    set(${out} "${_required}" PARENT_SCOPE)
endfunction()
