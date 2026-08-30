########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(httplib_FIND_QUIETLY)
    set(httplib_MESSAGE_MODE VERBOSE)
else()
    set(httplib_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/httplibTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${cpp-httplib_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(httplib_VERSION_STRING "0.47.0")
set(httplib_INCLUDE_DIRS ${cpp-httplib_INCLUDE_DIRS_RELWITHDEBINFO} )
set(httplib_INCLUDE_DIR ${cpp-httplib_INCLUDE_DIRS_RELWITHDEBINFO} )
set(httplib_LIBRARIES ${cpp-httplib_LIBRARIES_RELWITHDEBINFO} )
set(httplib_DEFINITIONS ${cpp-httplib_DEFINITIONS_RELWITHDEBINFO} )


# Definition of extra CMake variables from cmake_extra_variables


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${cpp-httplib_BUILD_MODULES_PATHS_RELWITHDEBINFO} )
    message(${httplib_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


