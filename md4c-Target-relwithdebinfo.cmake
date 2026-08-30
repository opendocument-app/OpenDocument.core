# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO "${md4c_FRAMEWORKS_RELWITHDEBINFO}" "${md4c_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(md4c_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET md4c_DEPS_TARGET)
    add_library(md4c_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET md4c_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${md4c_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:md4c::md4c>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### md4c_DEPS_TARGET to all of them
conan_package_library_targets("${md4c_LIBS_RELWITHDEBINFO}"    # libraries
                              "${md4c_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${md4c_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${md4c_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${md4c_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              md4c_DEPS_TARGET
                              md4c_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "md4c"    # package_name
                              "${md4c_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${md4c_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## COMPONENTS TARGET PROPERTIES RelWithDebInfo ########################################

    ########## COMPONENT md4c::md4c-html #############

        set(md4c_md4c_md4c-html_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(md4c_md4c_md4c-html_FRAMEWORKS_FOUND_RELWITHDEBINFO "${md4c_md4c_md4c-html_FRAMEWORKS_RELWITHDEBINFO}" "${md4c_md4c_md4c-html_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(md4c_md4c_md4c-html_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET md4c_md4c_md4c-html_DEPS_TARGET)
            add_library(md4c_md4c_md4c-html_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET md4c_md4c_md4c-html_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'md4c_md4c_md4c-html_DEPS_TARGET' to all of them
        conan_package_library_targets("${md4c_md4c_md4c-html_LIBS_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c-html_LIB_DIRS_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c-html_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${md4c_md4c_md4c-html_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c-html_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              md4c_md4c_md4c-html_DEPS_TARGET
                              md4c_md4c_md4c-html_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "md4c_md4c_md4c-html"
                              "${md4c_md4c_md4c-html_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET md4c::md4c-html
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_LIBRARIES_TARGETS}>
                     )

        if("${md4c_md4c_md4c-html_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET md4c::md4c-html
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         md4c_md4c_md4c-html_DEPS_TARGET)
        endif()

        set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c-html_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## COMPONENT md4c::md4c #############

        set(md4c_md4c_md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(md4c_md4c_md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO "${md4c_md4c_md4c_FRAMEWORKS_RELWITHDEBINFO}" "${md4c_md4c_md4c_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(md4c_md4c_md4c_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET md4c_md4c_md4c_DEPS_TARGET)
            add_library(md4c_md4c_md4c_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET md4c_md4c_md4c_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'md4c_md4c_md4c_DEPS_TARGET' to all of them
        conan_package_library_targets("${md4c_md4c_md4c_LIBS_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c_LIB_DIRS_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${md4c_md4c_md4c_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${md4c_md4c_md4c_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              md4c_md4c_md4c_DEPS_TARGET
                              md4c_md4c_md4c_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "md4c_md4c_md4c"
                              "${md4c_md4c_md4c_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET md4c::md4c
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_LIBRARIES_TARGETS}>
                     )

        if("${md4c_md4c_md4c_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET md4c::md4c
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         md4c_md4c_md4c_DEPS_TARGET)
        endif()

        set_property(TARGET md4c::md4c APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET md4c::md4c APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${md4c_md4c_md4c_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## AGGREGATED GLOBAL TARGET WITH THE COMPONENTS #####################
    set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_LINK_LIBRARIES md4c::md4c-html)
    set_property(TARGET md4c::md4c-html APPEND PROPERTY INTERFACE_LINK_LIBRARIES md4c::md4c)

########## For the modules (FindXXX)
set(md4c_LIBRARIES_RELWITHDEBINFO md4c::md4c-html)
