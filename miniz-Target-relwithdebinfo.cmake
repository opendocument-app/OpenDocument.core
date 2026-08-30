# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(miniz_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(miniz_FRAMEWORKS_FOUND_RELWITHDEBINFO "${miniz_FRAMEWORKS_RELWITHDEBINFO}" "${miniz_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(miniz_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET miniz_DEPS_TARGET)
    add_library(miniz_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET miniz_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${miniz_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${miniz_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### miniz_DEPS_TARGET to all of them
conan_package_library_targets("${miniz_LIBS_RELWITHDEBINFO}"    # libraries
                              "${miniz_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${miniz_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${miniz_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${miniz_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              miniz_DEPS_TARGET
                              miniz_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "miniz"    # package_name
                              "${miniz_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${miniz_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES RelWithDebInfo ########################################
    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:RelWithDebInfo>:${miniz_OBJECTS_RELWITHDEBINFO}>
                 $<$<CONFIG:RelWithDebInfo>:${miniz_LIBRARIES_TARGETS}>
                 )

    if("${miniz_LIBS_RELWITHDEBINFO}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET miniz::miniz
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     miniz_DEPS_TARGET)
    endif()

    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${miniz_LINKER_FLAGS_RELWITHDEBINFO}>)
    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${miniz_INCLUDE_DIRS_RELWITHDEBINFO}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${miniz_LIB_DIRS_RELWITHDEBINFO}>)
    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:RelWithDebInfo>:${miniz_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
    set_property(TARGET miniz::miniz
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${miniz_COMPILE_OPTIONS_RELWITHDEBINFO}>)

########## For the modules (FindXXX)
set(miniz_LIBRARIES_RELWITHDEBINFO miniz::miniz)
