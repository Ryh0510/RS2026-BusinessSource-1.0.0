include_guard(GLOBAL)

# Preserve existing build trees and command-line callers while showing only
# the new name in CMake GUI.
function(rs2026_named_cache_option new_name old_name help_text default_value)
    set(_last_name "RS2026_CACHE_LAST_${old_name}")
    set(_old_changed OFF)
    if(DEFINED CACHE{${new_name}} AND DEFINED CACHE{${old_name}} AND
       DEFINED CACHE{${_last_name}} AND
       NOT "$CACHE{${old_name}}" STREQUAL "$CACHE{${_last_name}}" AND
       "$CACHE{${new_name}}" STREQUAL "$CACHE{${_last_name}}")
        set(_old_changed ON)
    endif()
    if(DEFINED CACHE{${new_name}})
        set(_initial "$CACHE{${new_name}}")
    elseif(DEFINED CACHE{${old_name}})
        set(_initial "$CACHE{${old_name}}")
    else()
        set(_initial "${default_value}")
    endif()
    option(${new_name} "${help_text}" "${_initial}")
    if(_old_changed)
        set(${new_name} "$CACHE{${old_name}}" CACHE BOOL "${help_text}" FORCE)
    endif()
    set(${old_name} "${${new_name}}" CACHE INTERNAL
        "Compatibility alias for ${new_name}" FORCE)
    set(${_last_name} "${${new_name}}" CACHE INTERNAL
        "Last synchronized value for ${old_name}" FORCE)
endfunction()

function(rs2026_named_cache_option_with_legacy_alias new_name prior_name oldest_name help_text default_value)
    rs2026_named_cache_option(${prior_name} ${oldest_name}
        "Compatibility alias for ${new_name}" ${default_value})
    rs2026_named_cache_option(${new_name} ${prior_name}
        "${help_text}" ${default_value})
    # The second migration can change the value; keep both old cache inputs in sync.
    set(${oldest_name} "${${new_name}}" CACHE INTERNAL
        "Compatibility alias for ${new_name}" FORCE)
    set(RS2026_CACHE_LAST_${oldest_name} "${${new_name}}" CACHE INTERNAL
        "Last synchronized value for ${oldest_name}" FORCE)
endfunction()

function(rs2026_named_dependent_option new_name old_name help_text default_value dependency off_value)
    if(DEFINED CACHE{${new_name}})
        set(_initial "$CACHE{${new_name}}")
    elseif(DEFINED CACHE{${old_name}})
        set(_initial "$CACHE{${old_name}}")
    else()
        set(_initial "${default_value}")
    endif()
    cmake_dependent_option(${new_name} "${help_text}" "${_initial}" "${dependency}" "${off_value}")
    set(${old_name} "${${new_name}}" CACHE INTERNAL
        "Compatibility alias for ${new_name}" FORCE)
endfunction()

function(rs2026_named_cache_setting new_name old_name type help_text default_value)
    set(_last_name "RS2026_CACHE_LAST_${old_name}")
    set(_old_changed OFF)
    if(DEFINED CACHE{${new_name}} AND DEFINED CACHE{${old_name}} AND
       DEFINED CACHE{${_last_name}} AND
       NOT "$CACHE{${old_name}}" STREQUAL "$CACHE{${_last_name}}" AND
       "$CACHE{${new_name}}" STREQUAL "$CACHE{${_last_name}}")
        set(_old_changed ON)
    endif()
    if(DEFINED CACHE{${new_name}})
        set(_initial "$CACHE{${new_name}}")
    elseif(DEFINED CACHE{${old_name}})
        set(_initial "$CACHE{${old_name}}")
    else()
        set(_initial "${default_value}")
    endif()
    set(${new_name} "${_initial}" CACHE ${type} "${help_text}")
    if(_old_changed)
        set(${new_name} "$CACHE{${old_name}}" CACHE ${type} "${help_text}" FORCE)
    endif()
    set(${old_name} "${${new_name}}" CACHE INTERNAL
        "Compatibility alias for ${new_name}" FORCE)
    set(${_last_name} "${${new_name}}" CACHE INTERNAL
        "Last synchronized value for ${old_name}" FORCE)
endfunction()
