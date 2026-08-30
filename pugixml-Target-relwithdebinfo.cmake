# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(pugixml_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(pugixml_FRAMEWORKS_FOUND_RELWITHDEBINFO "${pugixml_FRAMEWORKS_RELWITHDEBINFO}" "${pugixml_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(pugixml_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET pugixml_DEPS_TARGET)
    add_library(pugixml_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET pugixml_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${pugixml_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${pugixml_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### pugixml_DEPS_TARGET to all of them
conan_package_library_targets("${pugixml_LIBS_RELWITHDEBINFO}"    # libraries
                              "${pugixml_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${pugixml_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${pugixml_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${pugixml_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              pugixml_DEPS_TARGET
                              pugixml_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "pugixml"    # package_name
                              "${pugixml_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${pugixml_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## GLOBAL TARGET PROPERTIES RelWithDebInfo ########################################
    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_OBJECTS_RELWITHDEBINFO}>
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_LIBRARIES_TARGETS}>
                 )

    if("${pugixml_LIBS_RELWITHDEBINFO}" STREQUAL "")
        # If the package is not declaring any "cpp_info.libs" the package deps, system libs,
        # frameworks etc are not linked to the imported targets and we need to do it to the
        # global target
        set_property(TARGET pugixml::pugixml
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     pugixml_DEPS_TARGET)
    endif()

    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_LINK_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_LINKER_FLAGS_RELWITHDEBINFO}>)
    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_INCLUDE_DIRS_RELWITHDEBINFO}>)
    # Necessary to find LINK shared libraries in Linux
    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_LIB_DIRS_RELWITHDEBINFO}>)
    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
    set_property(TARGET pugixml::pugixml
                 APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                 $<$<CONFIG:RelWithDebInfo>:${pugixml_COMPILE_OPTIONS_RELWITHDEBINFO}>)

########## For the modules (FindXXX)
set(pugixml_LIBRARIES_RELWITHDEBINFO pugixml::pugixml)
