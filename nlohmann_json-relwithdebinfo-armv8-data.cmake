########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(nlohmann_json_COMPONENT_NAMES "")
if(DEFINED nlohmann_json_FIND_DEPENDENCY_NAMES)
  list(APPEND nlohmann_json_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES nlohmann_json_FIND_DEPENDENCY_NAMES)
else()
  set(nlohmann_json_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(nlohmann_json_PACKAGE_FOLDER_RELWITHDEBINFO "/Users/andreas/.conan2/p/nlohmd014ef7748f4b/p")
set(nlohmann_json_BUILD_MODULES_PATHS_RELWITHDEBINFO )


set(nlohmann_json_INCLUDE_DIRS_RELWITHDEBINFO "${nlohmann_json_PACKAGE_FOLDER_RELWITHDEBINFO}/include")
set(nlohmann_json_RES_DIRS_RELWITHDEBINFO )
set(nlohmann_json_DEFINITIONS_RELWITHDEBINFO )
set(nlohmann_json_SHARED_LINK_FLAGS_RELWITHDEBINFO )
set(nlohmann_json_EXE_LINK_FLAGS_RELWITHDEBINFO )
set(nlohmann_json_OBJECTS_RELWITHDEBINFO )
set(nlohmann_json_COMPILE_DEFINITIONS_RELWITHDEBINFO )
set(nlohmann_json_COMPILE_OPTIONS_C_RELWITHDEBINFO )
set(nlohmann_json_COMPILE_OPTIONS_CXX_RELWITHDEBINFO )
set(nlohmann_json_LIB_DIRS_RELWITHDEBINFO )
set(nlohmann_json_BIN_DIRS_RELWITHDEBINFO )
set(nlohmann_json_LIBRARY_TYPE_RELWITHDEBINFO UNKNOWN)
set(nlohmann_json_IS_HOST_WINDOWS_RELWITHDEBINFO 0)
set(nlohmann_json_LIBS_RELWITHDEBINFO )
set(nlohmann_json_SYSTEM_LIBS_RELWITHDEBINFO )
set(nlohmann_json_FRAMEWORK_DIRS_RELWITHDEBINFO )
set(nlohmann_json_FRAMEWORKS_RELWITHDEBINFO )
set(nlohmann_json_BUILD_DIRS_RELWITHDEBINFO )
set(nlohmann_json_NO_SONAME_MODE_RELWITHDEBINFO FALSE)


# COMPOUND VARIABLES
set(nlohmann_json_COMPILE_OPTIONS_RELWITHDEBINFO
    "$<$<COMPILE_LANGUAGE:CXX>:${nlohmann_json_COMPILE_OPTIONS_CXX_RELWITHDEBINFO}>"
    "$<$<COMPILE_LANGUAGE:C>:${nlohmann_json_COMPILE_OPTIONS_C_RELWITHDEBINFO}>")
set(nlohmann_json_LINKER_FLAGS_RELWITHDEBINFO
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${nlohmann_json_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${nlohmann_json_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${nlohmann_json_EXE_LINK_FLAGS_RELWITHDEBINFO}>")


set(nlohmann_json_COMPONENTS_RELWITHDEBINFO )