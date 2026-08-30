########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(miniz_FIND_QUIETLY)
    set(miniz_MESSAGE_MODE VERBOSE)
else()
    set(miniz_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/minizTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${miniz_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(miniz_VERSION_STRING "3.1.1")
set(miniz_INCLUDE_DIRS ${miniz_INCLUDE_DIRS_RELWITHDEBINFO} )
set(miniz_INCLUDE_DIR ${miniz_INCLUDE_DIRS_RELWITHDEBINFO} )
set(miniz_LIBRARIES ${miniz_LIBRARIES_RELWITHDEBINFO} )
set(miniz_DEFINITIONS ${miniz_DEFINITIONS_RELWITHDEBINFO} )


# Definition of extra CMake variables from cmake_extra_variables


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${miniz_BUILD_MODULES_PATHS_RELWITHDEBINFO} )
    message(${miniz_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


