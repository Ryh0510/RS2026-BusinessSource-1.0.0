include_guard(GLOBAL)

function(rs2026_add_local_prebuilt_install_target)
    set(_selected_packages)
    set(_install_components)
    set(_build_targets)
    set(_problems)
    get_cmake_property(_known_components COMPONENTS)

    foreach(_package IN LISTS AllBuildPackages)
        if(NOT InstallPrebuilt_${_package})
            continue()
        endif()
        list(APPEND _selected_packages "${_package}")

        if(NOT BuildPackage_${_package} OR UsingPrebuilt_${_package})
            list(APPEND _problems
                "${_package}: enable its source BuildPackage option before local installation")
            continue()
        endif()
        if(NOT ExportPackage_${_package})
            list(APPEND _problems
                "${_package}: enable ExportPackage_${_package} to generate package metadata")
            continue()
        endif()

        set(_package_component "smrobot_sdk_${_package}_package")
        if(NOT _package_component IN_LIST _known_components)
            list(APPEND _problems
                "${_package}: package metadata has no dedicated install component")
            continue()
        endif()
        list(APPEND _install_components "${_package_component}")

        set(_library_count 0)
        foreach(_component IN LISTS ${_package}_ExportComponents)
            if(NOT TARGET ${_component})
                list(APPEND _problems
                    "${_package}::${_component}: build the exported source component")
                continue()
            endif()
            if(NOT ExportPackage_${_package}_${_component})
                list(APPEND _problems
                    "${_package}::${_component}: enable its ExportPackage option")
                continue()
            endif()
            get_target_property(_type ${_component} TYPE)
            if(NOT _type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|INTERFACE_LIBRARY)$")
                list(APPEND _problems
                    "${_package}::${_component}: ${_type} is not a prebuilt library")
                continue()
            endif()
            set(_component_install "smrobot_sdk_${_package}_${_component}")
            if(NOT _component_install IN_LIST _known_components)
                list(APPEND _problems
                    "${_package}::${_component}: no dedicated install component exists")
                continue()
            endif()
            list(APPEND _install_components "${_component_install}")
            math(EXPR _library_count "${_library_count} + 1")
            get_target_property(_aliased_target ${_component} ALIASED_TARGET)
            if(_aliased_target)
                set(_build_target "${_aliased_target}")
            else()
                set(_build_target "${_component}")
            endif()
            get_target_property(_imported ${_build_target} IMPORTED)
            if(NOT _imported AND NOT _type STREQUAL "INTERFACE_LIBRARY")
                list(APPEND _build_targets "${_build_target}")
            endif()
        endforeach()
        if(_library_count EQUAL 0)
            list(APPEND _problems "${_package}: no exportable library was selected")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _install_components)
    list(REMOVE_DUPLICATES _build_targets)
    if(_problems)
        set(_build_targets)
    endif()
    set(RS2026_LOCAL_PREBUILT_PACKAGES "${_selected_packages}")
    set(RS2026_LOCAL_PREBUILT_KNOWN_PACKAGES "${AllBuildPackages}")
    set(RS2026_LOCAL_PREBUILT_COMPONENTS "${_install_components}")
    set(RS2026_LOCAL_PREBUILT_PROBLEMS "${_problems}")
    set(RS2026_LOCAL_PREBUILT_BUILD_DIR "${CMAKE_BINARY_DIR}")
    set(RS2026_LOCAL_PREBUILT_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    set(RS2026_LOCAL_PREBUILT_ROOT "${CMAKE_INSTALL_PREFIX}")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RunSelectedPrebuiltInstall.cmake.in"
        "${CMAKE_BINARY_DIR}/RS2026RunSelectedPrebuiltInstall.cmake" @ONLY)
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RS2026DefaultInstallDispatcher.cmake.in"
        "${CMAKE_BINARY_DIR}/RS2026DefaultInstallDispatcher.cmake" @ONLY)

    add_custom_target(RS2026_InstallSelectedPrebuiltPackages
        COMMAND "${CMAKE_COMMAND}"
            "-DRS2026_LOCAL_PREBUILT_CONFIG=$<CONFIG>"
            -P "${CMAKE_BINARY_DIR}/RS2026RunSelectedPrebuiltInstall.cmake"
        DEPENDS ${_build_targets}
        USES_TERMINAL VERBATIM)
    set_target_properties(RS2026_InstallSelectedPrebuiltPackages PROPERTIES
        FOLDER "PackageManagement/Actions")
    message(STATUS
        "[LocalPrebuilt] Selected packages: ${_selected_packages}; output: ${CMAKE_INSTALL_PREFIX}")
endfunction()
