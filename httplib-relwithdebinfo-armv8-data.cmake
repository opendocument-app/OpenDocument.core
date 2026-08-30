########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(cpp-httplib_COMPONENT_NAMES "")
if(DEFINED cpp-httplib_FIND_DEPENDENCY_NAMES)
  list(APPEND cpp-httplib_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES cpp-httplib_FIND_DEPENDENCY_NAMES)
else()
  set(cpp-httplib_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(cpp-httplib_PACKAGE_FOLDER_RELWITHDEBINFO "/Users/andreas/.conan2/p/cpp-h320a4fd5aa060/p")
set(cpp-httplib_BUILD_MODULES_PATHS_RELWITHDEBINFO )


set(cpp-httplib_INCLUDE_DIRS_RELWITHDEBINFO "${cpp-httplib_PACKAGE_FOLDER_RELWITHDEBINFO}/include"
			"${cpp-httplib_PACKAGE_FOLDER_RELWITHDEBINFO}/include/httplib")
set(cpp-httplib_RES_DIRS_RELWITHDEBINFO )
set(cpp-httplib_DEFINITIONS_RELWITHDEBINFO "-DCPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO")
set(cpp-httplib_SHARED_LINK_FLAGS_RELWITHDEBINFO )
set(cpp-httplib_EXE_LINK_FLAGS_RELWITHDEBINFO )
set(cpp-httplib_OBJECTS_RELWITHDEBINFO )
set(cpp-httplib_COMPILE_DEFINITIONS_RELWITHDEBINFO "CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO")
set(cpp-httplib_COMPILE_OPTIONS_C_RELWITHDEBINFO )
set(cpp-httplib_COMPILE_OPTIONS_CXX_RELWITHDEBINFO )
set(cpp-httplib_LIB_DIRS_RELWITHDEBINFO )
set(cpp-httplib_BIN_DIRS_RELWITHDEBINFO )
set(cpp-httplib_LIBRARY_TYPE_RELWITHDEBINFO UNKNOWN)
set(cpp-httplib_IS_HOST_WINDOWS_RELWITHDEBINFO 0)
set(cpp-httplib_LIBS_RELWITHDEBINFO )
set(cpp-httplib_SYSTEM_LIBS_RELWITHDEBINFO )
set(cpp-httplib_FRAMEWORK_DIRS_RELWITHDEBINFO )
set(cpp-httplib_FRAMEWORKS_RELWITHDEBINFO CoreFoundation CFNetwork)
set(cpp-httplib_BUILD_DIRS_RELWITHDEBINFO )
set(cpp-httplib_NO_SONAME_MODE_RELWITHDEBINFO FALSE)


# COMPOUND VARIABLES
set(cpp-httplib_COMPILE_OPTIONS_RELWITHDEBINFO
    "$<$<COMPILE_LANGUAGE:CXX>:${cpp-httplib_COMPILE_OPTIONS_CXX_RELWITHDEBINFO}>"
    "$<$<COMPILE_LANGUAGE:C>:${cpp-httplib_COMPILE_OPTIONS_C_RELWITHDEBINFO}>")
set(cpp-httplib_LINKER_FLAGS_RELWITHDEBINFO
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${cpp-httplib_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${cpp-httplib_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${cpp-httplib_EXE_LINK_FLAGS_RELWITHDEBINFO}>")


set(cpp-httplib_COMPONENTS_RELWITHDEBINFO )