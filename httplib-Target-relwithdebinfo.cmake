# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(cpp-httplib_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(cpp-httplib_FRAMEWORKS_FOUND_RELWITHDEBINFO "${cpp-httplib_FRAMEWORKS_RELWITHDEBINFO}" "${cpp-httplib_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(cpp-httplib_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET cpp-httplib_DEPS_TARGET)
    add_library(cpp-httplib_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET cpp-httplib_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### cpp-httplib_DEPS_TARGET to all of them
conan_package_library_targets("${cpp-httplib_LIBS_RELWITHDEBINFO}"    # libraries
                              "${cpp-httplib_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${cpp-httplib_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${cpp-httplib_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${cpp-httplib_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              cpp-httplib_DEPS_TARGET
                              cpp-httplib_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "cpp-httplib"    # package_name
                              "${cpp-httplib_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${cpp-httplib_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES RelWithDebInfo ########################################
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_OBJECTS_RELWITHDEBINFO}>
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_LIBRARIES_TARGETS}>
                 )

    if("${cpp-httplib_LIBS_RELWITHDEBINFO}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET httplib::httplib
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     cpp-httplib_DEPS_TARGET)
    endif()

    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_LINKER_FLAGS_RELWITHDEBINFO}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_INCLUDE_DIRS_RELWITHDEBINFO}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_LIB_DIRS_RELWITHDEBINFO}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
    set_property(TARGET httplib::httplib
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${cpp-httplib_COMPILE_OPTIONS_RELWITHDEBINFO}>)

########## For the modules (FindXXX)
set(cpp-httplib_LIBRARIES_RELWITHDEBINFO httplib::httplib)
