########## MACROS ###########################################################################
#############################################################################################

# Requires CMake > 3.15
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeDeps' generator only works with CMake >= 3.15")
endif()

if(OpenJPEG_FIND_QUIETLY)
    set(OpenJPEG_MESSAGE_MODE VERBOSE)
else()
    set(OpenJPEG_MESSAGE_MODE STATUS)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmakedeps_macros.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/OpenJPEGTargets.cmake)
include(CMakeFindDependencyMacro)

check_build_type_defined()

foreach(_DEPENDENCY ${openjpeg_FIND_DEPENDENCY_NAMES} )
    # Check that we have not already called a find_package with the transitive dependency
    if(NOT ${_DEPENDENCY}_FOUND)
        find_dependency(${_DEPENDENCY} REQUIRED ${${_DEPENDENCY}_FIND_MODE})
    endif()
endforeach()

set(OpenJPEG_VERSION_STRING "2.5.4")
set(OpenJPEG_INCLUDE_DIRS ${openjpeg_INCLUDE_DIRS_RELWITHDEBINFO} )
set(OpenJPEG_INCLUDE_DIR ${openjpeg_INCLUDE_DIRS_RELWITHDEBINFO} )
set(OpenJPEG_LIBRARIES ${openjpeg_LIBRARIES_RELWITHDEBINFO} )
set(OpenJPEG_DEFINITIONS ${openjpeg_DEFINITIONS_RELWITHDEBINFO} )


# Definition of extra CMake variables from cmake_extra_variables


# Only the last installed configuration BUILD_MODULES are included to avoid the collision
foreach(_BUILD_MODULE ${openjpeg_BUILD_MODULES_PATHS_RELWITHDEBINFO} )
    message(${OpenJPEG_MESSAGE_MODE} "Conan: Including build module from '${_BUILD_MODULE}'")
    include(${_BUILD_MODULE})
endforeach()


