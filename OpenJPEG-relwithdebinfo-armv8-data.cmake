########### AGGREGATED COMPONENTS AND DEPENDENCIES FOR THE MULTI CONFIG #####################
#############################################################################################

set(openjpeg_COMPONENT_NAMES "")
if(DEFINED openjpeg_FIND_DEPENDENCY_NAMES)
  list(APPEND openjpeg_FIND_DEPENDENCY_NAMES )
  list(REMOVE_DUPLICATES openjpeg_FIND_DEPENDENCY_NAMES)
else()
  set(openjpeg_FIND_DEPENDENCY_NAMES )
endif()

########### VARIABLES #######################################################################
#############################################################################################
set(openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO "/Users/andreas/.conan2/p/b/openja715c9c610054/p")
set(openjpeg_BUILD_MODULES_PATHS_RELWITHDEBINFO "${openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO}/lib/cmake/conan-official-openjpeg-variables.cmake")


set(openjpeg_INCLUDE_DIRS_RELWITHDEBINFO "${openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO}/include"
			"${openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO}/include/openjpeg-2.5")
set(openjpeg_RES_DIRS_RELWITHDEBINFO )
set(openjpeg_DEFINITIONS_RELWITHDEBINFO )
set(openjpeg_SHARED_LINK_FLAGS_RELWITHDEBINFO )
set(openjpeg_EXE_LINK_FLAGS_RELWITHDEBINFO )
set(openjpeg_OBJECTS_RELWITHDEBINFO )
set(openjpeg_COMPILE_DEFINITIONS_RELWITHDEBINFO )
set(openjpeg_COMPILE_OPTIONS_C_RELWITHDEBINFO )
set(openjpeg_COMPILE_OPTIONS_CXX_RELWITHDEBINFO )
set(openjpeg_LIB_DIRS_RELWITHDEBINFO "${openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO}/lib")
set(openjpeg_BIN_DIRS_RELWITHDEBINFO )
set(openjpeg_LIBRARY_TYPE_RELWITHDEBINFO STATIC)
set(openjpeg_IS_HOST_WINDOWS_RELWITHDEBINFO 0)
set(openjpeg_LIBS_RELWITHDEBINFO openjp2)
set(openjpeg_SYSTEM_LIBS_RELWITHDEBINFO )
set(openjpeg_FRAMEWORK_DIRS_RELWITHDEBINFO )
set(openjpeg_FRAMEWORKS_RELWITHDEBINFO )
set(openjpeg_BUILD_DIRS_RELWITHDEBINFO "${openjpeg_PACKAGE_FOLDER_RELWITHDEBINFO}/lib/cmake")
set(openjpeg_NO_SONAME_MODE_RELWITHDEBINFO FALSE)


# COMPOUND VARIABLES
set(openjpeg_COMPILE_OPTIONS_RELWITHDEBINFO
    "$<$<COMPILE_LANGUAGE:CXX>:${openjpeg_COMPILE_OPTIONS_CXX_RELWITHDEBINFO}>"
    "$<$<COMPILE_LANGUAGE:C>:${openjpeg_COMPILE_OPTIONS_C_RELWITHDEBINFO}>")
set(openjpeg_LINKER_FLAGS_RELWITHDEBINFO
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,SHARED_LIBRARY>:${openjpeg_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,MODULE_LIBRARY>:${openjpeg_SHARED_LINK_FLAGS_RELWITHDEBINFO}>"
    "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${openjpeg_EXE_LINK_FLAGS_RELWITHDEBINFO}>")


set(openjpeg_COMPONENTS_RELWITHDEBINFO )