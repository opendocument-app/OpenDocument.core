# Avoid multiple calls to find_package to append duplicated properties to the targets
include_guard()########### VARIABLES #######################################################################
#############################################################################################
set(gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO "") # Will be filled later
conan_find_apple_frameworks(gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO "${gtest_FRAMEWORKS_RELWITHDEBINFO}" "${gtest_FRAMEWORK_DIRS_RELWITHDEBINFO}")

set(gtest_LIBRARIES_TARGETS "") # Will be filled later


######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
if(NOT TARGET gtest_DEPS_TARGET)
    add_library(gtest_DEPS_TARGET INTERFACE IMPORTED)
endif()

set_property(TARGET gtest_DEPS_TARGET
             APPEND PROPERTY INTERFACE_LINK_LIBRARIES
             $<$<CONFIG:RelWithDebInfo>:${gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:${gtest_SYSTEM_LIBS_RELWITHDEBINFO}>
             $<$<CONFIG:RelWithDebInfo>:GTest::gtest;GTest::gmock>)

####### Find the libraries declared in cpp_info.libs, create an IMPORTED target for each one and link the
####### gtest_DEPS_TARGET to all of them
conan_package_library_targets("${gtest_LIBS_RELWITHDEBINFO}"    # libraries
                              "${gtest_LIB_DIRS_RELWITHDEBINFO}" # package_libdir
                              "${gtest_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${gtest_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${gtest_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              gtest_DEPS_TARGET
                              gtest_LIBRARIES_TARGETS  # out_libraries_targets
                              "_RELWITHDEBINFO"
                              "gtest"    # package_name
                              "${gtest_NO_SONAME_MODE_RELWITHDEBINFO}")  # soname

# FIXME: What is the result of this for multi-config? All configs adding themselves to path?
set(CMAKE_MODULE_PATH ${gtest_BUILD_DIRS_RELWITHDEBINFO} ${CMAKE_MODULE_PATH})

########## COMPONENTS TARGET PROPERTIES RelWithDebInfo ########################################

    ########## COMPONENT GTest::gmock_main #############

        set(gtest_GTest_gmock_main_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(gtest_GTest_gmock_main_FRAMEWORKS_FOUND_RELWITHDEBINFO "${gtest_GTest_gmock_main_FRAMEWORKS_RELWITHDEBINFO}" "${gtest_GTest_gmock_main_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(gtest_GTest_gmock_main_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET gtest_GTest_gmock_main_DEPS_TARGET)
            add_library(gtest_GTest_gmock_main_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET gtest_GTest_gmock_main_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'gtest_GTest_gmock_main_DEPS_TARGET' to all of them
        conan_package_library_targets("${gtest_GTest_gmock_main_LIBS_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_main_LIB_DIRS_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_main_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${gtest_GTest_gmock_main_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_main_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              gtest_GTest_gmock_main_DEPS_TARGET
                              gtest_GTest_gmock_main_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "gtest_GTest_gmock_main"
                              "${gtest_GTest_gmock_main_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET GTest::gmock_main
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_LIBRARIES_TARGETS}>
                     )

        if("${gtest_GTest_gmock_main_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET GTest::gmock_main
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         gtest_GTest_gmock_main_DEPS_TARGET)
        endif()

        set_property(TARGET GTest::gmock_main APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock_main APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock_main APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock_main APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock_main APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_main_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## COMPONENT GTest::gmock #############

        set(gtest_GTest_gmock_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(gtest_GTest_gmock_FRAMEWORKS_FOUND_RELWITHDEBINFO "${gtest_GTest_gmock_FRAMEWORKS_RELWITHDEBINFO}" "${gtest_GTest_gmock_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(gtest_GTest_gmock_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET gtest_GTest_gmock_DEPS_TARGET)
            add_library(gtest_GTest_gmock_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET gtest_GTest_gmock_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'gtest_GTest_gmock_DEPS_TARGET' to all of them
        conan_package_library_targets("${gtest_GTest_gmock_LIBS_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_LIB_DIRS_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${gtest_GTest_gmock_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${gtest_GTest_gmock_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              gtest_GTest_gmock_DEPS_TARGET
                              gtest_GTest_gmock_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "gtest_GTest_gmock"
                              "${gtest_GTest_gmock_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET GTest::gmock
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_LIBRARIES_TARGETS}>
                     )

        if("${gtest_GTest_gmock_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET GTest::gmock
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         gtest_GTest_gmock_DEPS_TARGET)
        endif()

        set_property(TARGET GTest::gmock APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gmock APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gmock_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## COMPONENT GTest::gtest_main #############

        set(gtest_GTest_gtest_main_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(gtest_GTest_gtest_main_FRAMEWORKS_FOUND_RELWITHDEBINFO "${gtest_GTest_gtest_main_FRAMEWORKS_RELWITHDEBINFO}" "${gtest_GTest_gtest_main_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(gtest_GTest_gtest_main_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET gtest_GTest_gtest_main_DEPS_TARGET)
            add_library(gtest_GTest_gtest_main_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET gtest_GTest_gtest_main_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'gtest_GTest_gtest_main_DEPS_TARGET' to all of them
        conan_package_library_targets("${gtest_GTest_gtest_main_LIBS_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_main_LIB_DIRS_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_main_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${gtest_GTest_gtest_main_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_main_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              gtest_GTest_gtest_main_DEPS_TARGET
                              gtest_GTest_gtest_main_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "gtest_GTest_gtest_main"
                              "${gtest_GTest_gtest_main_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET GTest::gtest_main
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_LIBRARIES_TARGETS}>
                     )

        if("${gtest_GTest_gtest_main_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET GTest::gtest_main
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         gtest_GTest_gtest_main_DEPS_TARGET)
        endif()

        set_property(TARGET GTest::gtest_main APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest_main APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest_main APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest_main APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest_main APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_main_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## COMPONENT GTest::gtest #############

        set(gtest_GTest_gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO "")
        conan_find_apple_frameworks(gtest_GTest_gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO "${gtest_GTest_gtest_FRAMEWORKS_RELWITHDEBINFO}" "${gtest_GTest_gtest_FRAMEWORK_DIRS_RELWITHDEBINFO}")

        set(gtest_GTest_gtest_LIBRARIES_TARGETS "")

        ######## Create an interface target to contain all the dependencies (frameworks, system and conan deps)
        if(NOT TARGET gtest_GTest_gtest_DEPS_TARGET)
            add_library(gtest_GTest_gtest_DEPS_TARGET INTERFACE IMPORTED)
        endif()

        set_property(TARGET gtest_GTest_gtest_DEPS_TARGET
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_FRAMEWORKS_FOUND_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_SYSTEM_LIBS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_DEPENDENCIES_RELWITHDEBINFO}>
                     )

        ####### Find the libraries declared in cpp_info.component["xxx"].libs,
        ####### create an IMPORTED target for each one and link the 'gtest_GTest_gtest_DEPS_TARGET' to all of them
        conan_package_library_targets("${gtest_GTest_gtest_LIBS_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_LIB_DIRS_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_BIN_DIRS_RELWITHDEBINFO}" # package_bindir
                              "${gtest_GTest_gtest_LIBRARY_TYPE_RELWITHDEBINFO}"
                              "${gtest_GTest_gtest_IS_HOST_WINDOWS_RELWITHDEBINFO}"
                              gtest_GTest_gtest_DEPS_TARGET
                              gtest_GTest_gtest_LIBRARIES_TARGETS
                              "_RELWITHDEBINFO"
                              "gtest_GTest_gtest"
                              "${gtest_GTest_gtest_NO_SONAME_MODE_RELWITHDEBINFO}")


        ########## TARGET PROPERTIES #####################################
        set_property(TARGET GTest::gtest
                     APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_OBJECTS_RELWITHDEBINFO}>
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_LIBRARIES_TARGETS}>
                     )

        if("${gtest_GTest_gtest_LIBS_RELWITHDEBINFO}" STREQUAL "")
            # If the component is not declaring any "cpp_info.components['foo'].libs" the system, frameworks etc are not
            # linked to the imported targets and we need to do it to the global target
            set_property(TARGET GTest::gtest
                         APPEND PROPERTY INTERFACE_LINK_LIBRARIES
                         gtest_GTest_gtest_DEPS_TARGET)
        endif()

        set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_LINK_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_LINKER_FLAGS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_INCLUDE_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_LINK_DIRECTORIES
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_LIB_DIRS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_COMPILE_DEFINITIONS_RELWITHDEBINFO}>)
        set_property(TARGET GTest::gtest APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
                     $<$<CONFIG:RelWithDebInfo>:${gtest_GTest_gtest_COMPILE_OPTIONS_RELWITHDEBINFO}>)


    ########## AGGREGATED GLOBAL TARGET WITH THE COMPONENTS #####################
    set_property(TARGET gtest::gtest APPEND PROPERTY INTERFACE_LINK_LIBRARIES GTest::gmock_main)
    set_property(TARGET gtest::gtest APPEND PROPERTY INTERFACE_LINK_LIBRARIES GTest::gmock)
    set_property(TARGET gtest::gtest APPEND PROPERTY INTERFACE_LINK_LIBRARIES GTest::gtest_main)
    set_property(TARGET gtest::gtest APPEND PROPERTY INTERFACE_LINK_LIBRARIES GTest::gtest)

########## For the modules (FindXXX)
set(gtest_LIBRARIES_RELWITHDEBINFO gtest::gtest)
