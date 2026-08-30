# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(uchardet_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(uchardet_FRAMEWORKS_FOUND_RELWITHDEBINFO "${uchardet_FRAMEWORKS_RELWITHDEBINFO}" "${uchardet_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(uchardet_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET uchardet_DEPS_TARGET)
    add_library(uchardet_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET uchardet_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${uchardet_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${uchardet_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### uchardet_DEPS_TARGET to all of them
conan_package_library_targets("${uchardet_LIBS_RELWITHDEBINFO}"    # libraries
                              "${uchardet_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${uchardet_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${uchardet_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${uchardet_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              uchardet_DEPS_TARGET
                              uchardet_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "uchardet"    # package_name
                              "${uchardet_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${uchardet_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES RelWithDebInfo ########################################
    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_OBJECTS_RELWITHDEBINFO}>
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_LIBRARIES_TARGETS}>
                 )

    if("${uchardet_LIBS_RELWITHDEBINFO}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET uchardet::uchardet
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     uchardet_DEPS_TARGET)
    endif()

    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_LINKER_FLAGS_RELWITHDEBINFO}>)
    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_INCLUDE_DIRS_RELWITHDEBINFO}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_LIB_DIRS_RELWITHDEBINFO}>)
    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
    set_property(TARGET uchardet::uchardet
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${uchardet_COMPILE_OPTIONS_RELWITHDEBINFO}>)

########## For the modules (FindXXX)
set(uchardet_LIBRARIES_RELWITHDEBINFO uchardet::uchardet)
