# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(nlohmann_json_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(nlohmann_json_FRAMEWORKS_FOUND_RELWITHDEBINFO "${nlohmann_json_FRAMEWORKS_RELWITHDEBINFO}" "${nlohmann_json_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(nlohmann_json_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET nlohmann_json_DEPS_TARGET)
    add_library(nlohmann_json_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET nlohmann_json_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### nlohmann_json_DEPS_TARGET to all of them
conan_package_library_targets("${nlohmann_json_LIBS_RELWITHDEBINFO}"    # libraries
                              "${nlohmann_json_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${nlohmann_json_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${nlohmann_json_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${nlohmann_json_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              nlohmann_json_DEPS_TARGET
                              nlohmann_json_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "nlohmann_json"    # package_name
                              "${nlohmann_json_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${nlohmann_json_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES RelWithDebInfo ########################################
    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_OBJECTS_RELWITHDEBINFO}>
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_LIBRARIES_TARGETS}>
                 )

    if("${nlohmann_json_LIBS_RELWITHDEBINFO}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET nlohmann_json::nlohmann_json
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     nlohmann_json_DEPS_TARGET)
    endif()

    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_LINKER_FLAGS_RELWITHDEBINFO}>)
    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_INCLUDE_DIRS_RELWITHDEBINFO}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_LIB_DIRS_RELWITHDEBINFO}>)
    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
    set_property(TARGET nlohmann_json::nlohmann_json
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${nlohmann_json_COMPILE_OPTIONS_RELWITHDEBINFO}>)

########## For the modules (FindXXX)
set(nlohmann_json_LIBRARIES_RELWITHDEBINFO nlohmann_json::nlohmann_json)
