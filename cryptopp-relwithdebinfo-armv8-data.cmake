########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

list(APPEND cryptopp_COMPONENT_NAMES cryptopp::cryptopp)
list(REMOVE_DUPLICATES cryptopp_COMPONENT_NAMES)
if(DEFINED cryptopp_FIND_DEPENDENCY_NAMES)
  list(APPEND cryptopp_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES cryptopp_FIND_DEPENDENCY_NAMES)
else()
  set(cryptopp_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(cryptopp_PACKAGE_FOLDER_RELWITHDEBINFO "/Users/andreas/.conan2/p/b/crypt4ff9b27e63670/p")
set(cryptopp_BUILD_MODULES_PATHS_RELWITHDEBINFO )


set(cryptopp_INCLUDE_DIRS_RELWITHDEBINFO "${cryptopp_PACKAGE_FOLDER_RELWITHDEBINFO}/include")
set(cryptopp_RES_DIRS_RELWITHDEBINFO )
set(cryptopp_DEFINITIONS_RELWITHDEBINFO )
set(cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO )
set(cryptopp_EXE_LINK_FLAGS_RELWITHDEBINFO )
set(cryptopp_OBJECTS_RELWITHDEBINFO )
set(cryptopp_COMPILE_DEFINITIONS_RELWITHDEBINFO )
set(cryptopp_COMPILE_OPTIONS_C_RELWITHDEBINFO )
set(cryptopp_COMPILE_OPTIONS_CXX_RELWITHDEBINFO )
set(cryptopp_LIB_DIRS_RELWITHDEBINFO "${cryptopp_PACKAGE_FOLDER_RELWITHDEBINFO}/lib")
set(cryptopp_BIN_DIRS_RELWITHDEBINFO )
set(cryptopp_LIBRARY_TYPE_RELWITHDEBINFO STATIC)
set(cryptopp_IS_HOST_WINDOWS_RELWITHDEBINFO 0)
set(cryptopp_LIBS_RELWITHDEBINFO cryptopp)
set(cryptopp_SYSTEM_LIBS_RELWITHDEBINFO )
set(cryptopp_FRAMEWORK_DIRS_RELWITHDEBINFO )
set(cryptopp_FRAMEWORKS_RELWITHDEBINFO )
set(cryptopp_BUILD_DIRS_RELWITHDEBINFO )
set(cryptopp_NO_SONAME_MODE_RELWITHDEBINFO FALSE)


# COMPOUND VARIABLES
set(cryptopp_COMPILE_OPTIONS_RELWITHDEBINFO
    "$<$<COMPILE_LANGUAGE:CXX>:${cryptopp_COMPILE_OPTIONS_CXX_RELWITHDEBINFO}>"
    "$<$<COMPILE_LANGUAGE:C>:${cryptopp_COMPILE_OPTIONS_C_RELWITHDEBINFO}>")
set(cryptopp_LINKER_FLAGS_RELWITHDEBINFO
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${cryptopp_EXE_LINK_FLAGS_RELWITHDEBINFO}>")


set(cryptopp_COMPONENTS_RELWITHDEBINFO cryptopp::cryptopp)
########### COMPONENT cryptopp::cryptopp VARIABLES ############################################

set(cryptopp_cryptopp_cryptopp_INCLUDE_DIRS_RELWITHDEBINFO "${cryptopp_PACKAGE_FOLDER_RELWITHDEBINFO}/include")
set(cryptopp_cryptopp_cryptopp_LIB_DIRS_RELWITHDEBINFO "${cryptopp_PACKAGE_FOLDER_RELWITHDEBINFO}/lib")
set(cryptopp_cryptopp_cryptopp_BIN_DIRS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_LIBRARY_TYPE_RELWITHDEBINFO STATIC)
set(cryptopp_cryptopp_cryptopp_IS_HOST_WINDOWS_RELWITHDEBINFO 0)
set(cryptopp_cryptopp_cryptopp_RES_DIRS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_DEFINITIONS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_OBJECTS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_COMPILE_DEFINITIONS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_COMPILE_OPTIONS_C_RELWITHDEBINFO "")
set(cryptopp_cryptopp_cryptopp_COMPILE_OPTIONS_CXX_RELWITHDEBINFO "")
set(cryptopp_cryptopp_cryptopp_LIBS_RELWITHDEBINFO cryptopp)
set(cryptopp_cryptopp_cryptopp_SYSTEM_LIBS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_FRAMEWORK_DIRS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_FRAMEWORKS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_DEPENDENCIES_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_EXE_LINK_FLAGS_RELWITHDEBINFO )
set(cryptopp_cryptopp_cryptopp_NO_SONAME_MODE_RELWITHDEBINFO FALSE)

# COMPOUND VARIABLES
set(cryptopp_cryptopp_cryptopp_LINKER_FLAGS_RELWITHDEBINFO
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${cryptopp_cryptopp_cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${cryptopp_cryptopp_cryptopp_SHARED_LINK_FLAGS_RELWITHDEBINFO}>
        $<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${cryptopp_cryptopp_cryptopp_EXE_LINK_FLAGS_RELWITHDEBINFO}>
)
set(cryptopp_cryptopp_cryptopp_COMPILE_OPTIONS_RELWITHDEBINFO
    "$<$<COMPILE_LANGUAGE:CXX>:${cryptopp_cryptopp_cryptopp_COMPILE_OPTIONS_CXX_RELWITHDEBINFO}>"
    "$<$<COMPILE_LANGUAGE:C>:${cryptopp_cryptopp_cryptopp_COMPILE_OPTIONS_C_RELWITHDEBINFO}>")