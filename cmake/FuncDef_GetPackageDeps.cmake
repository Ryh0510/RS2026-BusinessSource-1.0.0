############################################################
#   Function - parse_required_libs
#   Usage:
#   parse_required_libs(${PACKAGE_NAME}_RequiredLibs)       #   Not used now
#   resolve_dependencies(${PACKAGE_NAME}_RequiredLibs)      #   Same as parse_required_libs.
############################################################

# function(parse_required_libs libs_var)

#     foreach(lib ${${libs_var}})
#         string(REPLACE "::" ";" parts ${lib})
#         list(GET parts 0 pkg)
#         list(GET parts 1 comp)

#      This scope collects  
#         list(APPEND ${pkg}_Components_local ${comp})
#         list(APPEND _all_pkgs ${pkg})
#     endforeach()


#     foreach(pkg ${_all_pkgs})
# Deduplication
#         list(REMOVE_DUPLICATES ${pkg}_Components_local)
#         message(STATUS "Finding ${pkg} with components: ${${pkg}_Components_local}")
        
# Export to the parent scope (if needed)
#         set(${pkg}_Components ${${pkg}_Components_local} PARENT_SCOPE)
#     endforeach()

# endfunction()



function(_rs_normalize_dependency dependency out_dependency out_state)
    set(_dependency "${dependency}")
    set(_state VALID)

    # TargetConfigSetting files describe source-build dependencies. Unwrap the
    # generator expressions that can safely be interpreted at configure time.
    while(TRUE)
        if(_dependency MATCHES "^\\$<BUILD_INTERFACE:(.+)>$")
            set(_dependency "${CMAKE_MATCH_1}")
        elseif(_dependency MATCHES "^\\$<LINK_ONLY:(.+)>$")
            set(_dependency "${CMAKE_MATCH_1}")
        elseif(_dependency MATCHES "^\\$<INSTALL_INTERFACE:(.+)>$")
            set(_dependency "")
            set(_state SKIP)
            break()
        else()
            break()
        endif()
    endwhile()

    if(_state STREQUAL "VALID" AND
       NOT _dependency MATCHES "^[A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+$")
        set(_state INVALID)
    endif()

    set(${out_dependency} "${_dependency}" PARENT_SCOPE)
    set(${out_state} "${_state}" PARENT_SCOPE)
endfunction()


function(resolve_dependencies libs_var)
    set(_all_pkgs)
    set(_snapshot_complete ON)

    # Clear compatibility outputs from an earlier call in the same configure.
    foreach(pkg IN LISTS AllPackagesRequired)
        unset(${pkg}_DepLibs PARENT_SCOPE)
        unset(RS2026_REQUIRED_COMPONENTS_${pkg} PARENT_SCOPE)
    endforeach()

    foreach(lib IN LISTS ${libs_var})
        _rs_normalize_dependency("${lib}" _dependency _dependency_state)

        if(_dependency_state STREQUAL "SKIP")
            continue()
        elseif(_dependency_state STREQUAL "INVALID")
            message(WARNING
                "[DependencySnapshot] Ignore unsupported dependency metadata '${lib}'. "
                "Demand loading will remain conservative for this configure.")
            set(_snapshot_complete OFF)
            continue()
        endif()

        string(REPLACE "::" ";" _dependency_parts "${_dependency}")
        list(GET _dependency_parts 0 pkg)
        list(GET _dependency_parts 1 comp)

        list(APPEND ${pkg}_deplibs_local "${comp}")
        list(APPEND _all_pkgs "${pkg}")
    endforeach()

    list(REMOVE_DUPLICATES _all_pkgs)

    foreach(pkg IN LISTS _all_pkgs)
        list(REMOVE_DUPLICATES ${pkg}_deplibs_local)
        message(STATUS
            "Require Package ${pkg} with components: ${${pkg}_deplibs_local}")

        # Preserve the original outputs while exposing a stable read-only
        # dependency snapshot for user-specific providers.
        set(${pkg}_DepLibs "${${pkg}_deplibs_local}" PARENT_SCOPE)
        set(RS2026_REQUIRED_COMPONENTS_${pkg}
            "${${pkg}_deplibs_local}" PARENT_SCOPE)
    endforeach()

    set(AllPackagesRequired "${_all_pkgs}" PARENT_SCOPE)
    set(RS2026_REQUIRED_PACKAGES "${_all_pkgs}" PARENT_SCOPE)
    set(RS2026_DEPENDENCY_SNAPSHOT_COMPLETE
        "${_snapshot_complete}" PARENT_SCOPE)
endfunction()


function(rs_project_requires_package package out_required)
    list(FIND RS2026_REQUIRED_PACKAGES "${package}" _package_index)
    if(_package_index EQUAL -1)
        set(${out_required} OFF PARENT_SCOPE)
    else()
        set(${out_required} ON PARENT_SCOPE)
    endif()
endfunction()


function(rs_project_required_components package out_components)
    set(${out_components}
        "${RS2026_REQUIRED_COMPONENTS_${package}}" PARENT_SCOPE)
endfunction()


function(rs_project_requires_component package component out_required)
    list(FIND RS2026_REQUIRED_COMPONENTS_${package}
        "${component}" _component_index)
    if(_component_index EQUAL -1)
        set(${out_required} OFF PARENT_SCOPE)
    else()
        set(${out_required} ON PARENT_SCOPE)
    endif()
endfunction()


function(rs_project_requires_any_package out_required)
    set(_required OFF)
    foreach(package IN LISTS ARGN)
        rs_project_requires_package("${package}" _package_required)
        if(_package_required)
            set(_required ON)
            break()
        endif()
    endforeach()
    set(${out_required} "${_required}" PARENT_SCOPE)
endfunction()


function(rs_assert_required_targets provider_name)
    foreach(required_target IN LISTS ARGN)
        if(NOT TARGET "${required_target}")
            message(FATAL_ERROR
                "[DependencyProvider] ${provider_name} did not provide required target "
                "${required_target}.")
        endif()
    endforeach()
endfunction()


#   Use for both debug and release dlls

function(add_dll_path_from_target target imported)

    get_target_property(_dll ${imported} IMPORTED_LOCATION_DEBUG)

    if(_dll)
        get_filename_component(_dir ${_dll} DIRECTORY)

        set_property(TARGET ${target}
            APPEND PROPERTY
            VS_DEBUGGER_ENVIRONMENT
            "PATH=${_dir};%PATH%"
        )
    endif()

endfunction()

#   add_dll_path_from_target(coal_test coal::coal)

# Keep declaration collection and source construction on the same cache choices.
function(rs_component_is_enabled package component out_enabled)
    set(_enabled OFF)
    if(BuildPackage_${package} AND NOT UsingPrebuilt_${package})
        if(package STREQUAL "SMRobotApps" AND component STREQUAL "RobotViewerCore")
            # RobotViewerCore is deliberately always added by the Apps source package.
            set(_enabled ON)
        else()
            option(Build${package}_${component} "Build ${package}::${component}" ON)
            if(Build${package}_${component})
                set(_enabled ON)
            endif()
            if(package STREQUAL "SMRobotApps" AND component STREQUAL "RobotQtViewer"
               AND NOT BuildRobotQtViewer)
                set(_enabled OFF)
            endif()
        endif()
    endif()
    set(${out_enabled} "${_enabled}" PARENT_SCOPE)
endfunction()

macro(rs_begin_package_requirements package)
    set(${package}_RequiredLibsPublic)
    set(${package}_RequiredLibsPrivate)
    set(${package}_RequiredLibs)
    set(RS2026_SOURCE_COMPONENTS_${package})
endmacro()

macro(rs_collect_component_requirements package component)
    rs_component_is_enabled("${package}" "${component}" _rs_component_enabled)
    if(_rs_component_enabled)
        list(APPEND RS2026_SOURCE_COMPONENTS_${package} "${component}")
        list(APPEND ${package}_RequiredLibsPublic ${${component}_RequiredLibsPublic})
        list(APPEND ${package}_RequiredLibsPrivate ${${component}_RequiredLibsPrivate})
    endif()
    unset(_rs_component_enabled)
endmacro()

function(rs_source_dependency_owner package component out_package out_component)
    set(_package "${package}")
    set(_component "${component}")
    if(package STREQUAL "SMRobotWorkbenchCommon" AND
       component MATCHES "^RobotQtModules(Shared|CoreWidgets)$")
        set(_package SMRobotApps)
        set(_component RobotQtModules)
    elseif(package STREQUAL "SMRobotCore" AND
           component MATCHES "^Collision(Robot|Tools|Presentation)$")
        set(_component Collision)
    elseif(package STREQUAL "SMRobotWorkbenchProjectAssembly" AND
           component MATCHES "^ProjectAssembly(Editors|SceneExplorer|ToolSetup)$")
        set(_component ProjectAssemblyWorkbench)
    elseif(package STREQUAL "SMRobotWorkbenchCollisionConfig" AND
           component MATCHES "^Collision(DetectorConfig|LinkModelSetup|RuntimeResults)$")
        set(_component CollisionConfigWorkbench)
    elseif(package STREQUAL "SMRobotWorkbenchRobotRun" AND
           component STREQUAL "RobotRunMotionControl")
        set(_component RobotRunWorkbench)
    elseif(package STREQUAL "SMRobotWorkbenchMotionPlanning" AND
           component STREQUAL "MotionPlanningEditor")
        set(_component MotionPlanningWorkbench)
    endif()
    set(${out_package} "${_package}" PARENT_SCOPE)
    set(${out_component} "${_component}" PARENT_SCOPE)
endfunction()

function(rs_validate_source_component_dependencies)
    foreach(_source_package IN LISTS AllBuildPackages)
        foreach(_source_component IN LISTS RS2026_SOURCE_COMPONENTS_${_source_package})
            set(_dependencies
                ${${_source_package}${_source_component}_RequiredLibsPublic}
                ${${_source_package}${_source_component}_RequiredLibsPrivate})
            foreach(_raw IN LISTS _dependencies)
                _rs_normalize_dependency("${_raw}" _dependency _state)
                if(NOT _state STREQUAL "VALID")
                    continue()
                endif()
                string(REPLACE "::" ";" _parts "${_dependency}")
                list(GET _parts 0 _package)
                list(GET _parts 1 _component)
                if(NOT _package IN_LIST AllBuildPackages)
                    continue()
                endif()
                if(UsingPrebuilt_${_package})
                    continue()
                endif()
                rs_source_dependency_owner("${_package}" "${_component}" _owner _owned)
                rs_component_is_enabled("${_owner}" "${_owned}" _enabled)
                if(NOT _enabled)
                    message(FATAL_ERROR
                        "[BuildGraph] ${_source_package}::${_source_component} requires "
                        "${_dependency}, but its source owner ${_owner}::${_owned} is disabled. "
                        "Enable BuildPackage_${_owner} and Build${_owner}_${_owned}, "
                        "select an appropriate prebuilt package, or disable the caller.")
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()
