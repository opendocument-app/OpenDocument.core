########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(nlohmann_json_FIND_QUIETLY)
    set(nlohmann_json_MESSAGE_MODE VERBOSE)
else()
    set(nlohmann_json_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/nlohmann_jsonTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${nlohmann_json_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(nlohmann_json_VERSION_STRING "3.12.0")
set(nlohmann_json_INCLUDE_DIRS ${nlohmann_json_INCLUDE_DIRS_RELWITHDEBINFO} )
set(nlohmann_json_INCLUDE_DIR ${nlohmann_json_INCLUDE_DIRS_RELWITHDEBINFO} )
set(nlohmann_json_LIBRARIES ${nlohmann_json_LIBRARIES_RELWITHDEBINFO} )
set(nlohmann_json_DEFINITIONS ${nlohmann_json_DEFINITIONS_RELWITHDEBINFO} )


# Definition of extra CMake variables from cmake_extra_variables


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${nlohmann_json_BUILD_MODULES_PATHS_RELWITHDEBINFO} )
    message(${nlohmann_json_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


