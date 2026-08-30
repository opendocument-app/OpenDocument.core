# Conan automatically generated toolchain file
# DO NOT EDIT MANUALLY, it will be overwritten

# Avoid including toolchain file several times (bad if appending to variables like
#   CMAKE_CXX_FLAGS. See https://github.com/android/ndk/issues/323
include_guard()
message(STATUS "Using Conan toolchain: ${CMAKE_CURRENT_LIST_FILE}")
if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeToolchain' generator only works with CMake >= 3.15")
endif()

########## 'user_toolchain' block #############
# Include one or more CMake user toolchain from tools.cmake.cmaketoolchain:user_toolchain



########## 'generic_system' block #############
# Definition of system, platform and toolset





########## 'compilers' block #############

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")


########## 'apple_system' block #############
# Define Apple architectures, sysroot, deployment target, bitcode, etc

# Set the architectures for which to build.
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
# Setting CMAKE_OSX_SYSROOT SDK, when using Xcode generator the name is enough
# but full path is necessary for others
set(CMAKE_OSX_SYSROOT macosx CACHE STRING "" FORCE)
set(BITCODE "")
set(FOBJC_ARC "")
set(VISIBILITY "")
#Check if Xcode generator is used, since that will handle these flags automagically
if(CMAKE_GENERATOR MATCHES "Xcode")
  message(DEBUG "Not setting any manual command-line buildflags, since Xcode is selected as generator.")
else()
    string(APPEND CONAN_C_FLAGS " ${BITCODE} ${VISIBILITY}")
    string(APPEND CONAN_CXX_FLAGS " ${BITCODE} ${VISIBILITY}")
    # Objective-C/C++ specific flags
    string(APPEND CONAN_OBJC_FLAGS " ${BITCODE} ${VISIBILITY} ${FOBJC_ARC}")
    string(APPEND CONAN_OBJCXX_FLAGS " ${BITCODE} ${VISIBILITY} ${FOBJC_ARC}")
endif()


########## 'fpic' block #############
# Defining CMAKE_POSITION_INDEPENDENT_CODE for static libraries when necessary

message(STATUS "Conan toolchain: Setting CMAKE_POSITION_INDEPENDENT_CODE=ON (options.fPIC)")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Position independent code")


########## 'rpath_link_flags' block #############
# Pass -rpath-link pointing to all directories with runtime libraries


########## 'libcxx' block #############
# Definition of libcxx from 'compiler.libcxx' setting, defining the
# right CXX_FLAGS for that libcxx

message(STATUS "Conan toolchain: Defining libcxx as C++ flags: -stdlib=libc++")
string(APPEND CONAN_CXX_FLAGS " -stdlib=libc++")


########## 'cppstd' block #############
# Define the C++ and C standards from 'compiler.cppstd' and 'compiler.cstd'

function(conan_modify_std_watch variable access value current_list_file stack)
    set(conan_watched_std_variable "20")
    if (${variable} STREQUAL "CMAKE_C_STANDARD")
        set(conan_watched_std_variable "")
    endif()
    if ("${access}" STREQUAL "MODIFIED_ACCESS" AND NOT "${value}" STREQUAL "${conan_watched_std_variable}")
        message(STATUS "Warning: Standard ${variable} value defined in conan_toolchain.cmake to ${conan_watched_std_variable} has been modified to ${value} by ${current_list_file}")
    endif()
    unset(conan_watched_std_variable)
endfunction()

message(STATUS "Conan toolchain: C++ Standard 20 with extensions ON")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
variable_watch(CMAKE_CXX_STANDARD conan_modify_std_watch)


########## 'extra_flags' block #############
# Include extra C++, C and linker flags from configuration tools.build:<type>flags
# and from CMakeToolchain.extra_<type>_flags

# Conan conf flags start: 
string(APPEND CONAN_CXX_FLAGS " -fno-sanitize=all")
string(APPEND CONAN_C_FLAGS " -fno-sanitize=all")
# Conan conf flags end


########## 'cmake_flags_init' block #############
# Define CMAKE_<XXX>_FLAGS from CONAN_<XXX>_FLAGS

foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER ${config} config)
    if(DEFINED CONAN_CXX_FLAGS_${config})
      string(APPEND CMAKE_CXX_FLAGS_${config}_INIT " ${CONAN_CXX_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_C_FLAGS_${config})
      string(APPEND CMAKE_C_FLAGS_${config}_INIT " ${CONAN_C_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_SHARED_LINKER_FLAGS_${config})
      string(APPEND CMAKE_SHARED_LINKER_FLAGS_${config}_INIT " ${CONAN_SHARED_LINKER_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_EXE_LINKER_FLAGS_${config})
      string(APPEND CMAKE_EXE_LINKER_FLAGS_${config}_INIT " ${CONAN_EXE_LINKER_FLAGS_${config}}")
    endif()
endforeach()

if(DEFINED CONAN_CXX_FLAGS)
  string(APPEND CMAKE_CXX_FLAGS_INIT " ${CONAN_CXX_FLAGS}")
endif()
if(DEFINED CONAN_C_FLAGS)
  string(APPEND CMAKE_C_FLAGS_INIT " ${CONAN_C_FLAGS}")
endif()
if(DEFINED CONAN_SHARED_LINKER_FLAGS)
  string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " ${CONAN_SHARED_LINKER_FLAGS}")
endif()
if(DEFINED CONAN_EXE_LINKER_FLAGS)
  string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " ${CONAN_EXE_LINKER_FLAGS}")
endif()
if(DEFINED CONAN_OBJCXX_FLAGS)
  string(APPEND CMAKE_OBJCXX_FLAGS_INIT " ${CONAN_OBJCXX_FLAGS}")
endif()
if(DEFINED CONAN_OBJC_FLAGS)
  string(APPEND CMAKE_OBJC_FLAGS_INIT " ${CONAN_OBJC_FLAGS}")
endif()


########## 'extra_variables' block #############
# Definition of extra CMake variables from tools.cmake.cmaketoolchain:extra_variables



########## 'try_compile' block #############
# Blocks after this one will not be added when running CMake try/checks
get_property( _CMAKE_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if(_CMAKE_IN_TRY_COMPILE)
    message(STATUS "Running toolchain IN_TRY_COMPILE")
    return()
endif()


########## 'find_paths' block #############
# Define paths to find packages, programs, libraries, etc.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/conan_cmakedeps_paths.cmake")
  message(STATUS "Conan toolchain: Including CMakeDeps generated conan_cmakedeps_paths.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/conan_cmakedeps_paths.cmake")
else()

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# Definition of CMAKE_MODULE_PATH
list(PREPEND CMAKE_MODULE_PATH "/Users/andreas/.conan2/p/b/openja715c9c610054/p/lib/cmake")
# the generators folder (where conan generates files, like this toolchain)
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# Definition of CMAKE_PREFIX_PATH, CMAKE_XXXXX_PATH
# The explicitly defined "builddirs" of "host" context dependencies must be in PREFIX_PATH
list(PREPEND CMAKE_PREFIX_PATH "/Users/andreas/.conan2/p/b/openja715c9c610054/p/lib/cmake")
# The Conan local "generators" folder, where this toolchain is saved.
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR} )
list(PREPEND CMAKE_LIBRARY_PATH "/Users/andreas/.conan2/p/b/crypt4ff9b27e63670/p/lib" "/Users/andreas/.conan2/p/b/md4c0a60c9bc55651/p/lib" "/Users/andreas/.conan2/p/b/miniz5c80580ddaacd/p/lib" "/Users/andreas/.conan2/p/b/openja715c9c610054/p/lib" "/Users/andreas/.conan2/p/b/uchar435bc08c03cd6/p/lib" "/Users/andreas/.conan2/p/b/gtestb6bb409d34b28/p/lib")
list(PREPEND CMAKE_INCLUDE_PATH "/Users/andreas/.conan2/p/pugix676f7d1fa98b4/p/include" "/Users/andreas/.conan2/p/b/crypt4ff9b27e63670/p/include" "/Users/andreas/.conan2/p/b/md4c0a60c9bc55651/p/include" "/Users/andreas/.conan2/p/b/miniz5c80580ddaacd/p/include" "/Users/andreas/.conan2/p/b/miniz5c80580ddaacd/p/include/miniz" "/Users/andreas/.conan2/p/nlohmd014ef7748f4b/p/include" "/Users/andreas/.conan2/p/b/openja715c9c610054/p/include" "/Users/andreas/.conan2/p/b/openja715c9c610054/p/include/openjpeg-2.5" "/Users/andreas/.conan2/p/b/uchar435bc08c03cd6/p/include" "/Users/andreas/.conan2/p/utfcp412baeeda02ec/p/include" "/Users/andreas/.conan2/p/utfcp412baeeda02ec/p/include/utf8cpp" "/Users/andreas/.conan2/p/cpp-h320a4fd5aa060/p/include" "/Users/andreas/.conan2/p/cpp-h320a4fd5aa060/p/include/httplib" "/Users/andreas/.conan2/p/b/gtestb6bb409d34b28/p/include")
set(CONAN_RUNTIME_LIB_DIRS "/Users/andreas/.conan2/p/b/crypt4ff9b27e63670/p/lib" "/Users/andreas/.conan2/p/b/md4c0a60c9bc55651/p/lib" "/Users/andreas/.conan2/p/b/miniz5c80580ddaacd/p/lib" "/Users/andreas/.conan2/p/b/openja715c9c610054/p/lib" "/Users/andreas/.conan2/p/b/uchar435bc08c03cd6/p/lib" "/Users/andreas/.conan2/p/b/gtestb6bb409d34b28/p/lib" )

endif()


########## 'pkg_config' block #############
# Define pkg-config from 'tools.gnu:pkg_config' executable and paths

if (DEFINED ENV{PKG_CONFIG_PATH})
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_LIST_DIR}:$ENV{PKG_CONFIG_PATH}")
else()
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_LIST_DIR}:")
endif()


########## 'rpath' block #############
# Defining CMAKE_SKIP_RPATH



########## 'shared' block #############
# Define BUILD_SHARED_LIBS for shared libraries

message(STATUS "Conan toolchain: Setting BUILD_SHARED_LIBS = OFF")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")


########## 'output_dirs' block #############
# Definition of CMAKE_INSTALL_XXX folders

set(CMAKE_INSTALL_BINDIR "bin")
set(CMAKE_INSTALL_SBINDIR "bin")
set(CMAKE_INSTALL_LIBEXECDIR "bin")
set(CMAKE_INSTALL_LIBDIR "lib")
set(CMAKE_INSTALL_INCLUDEDIR "include")
set(CMAKE_INSTALL_OLDINCLUDEDIR "include")


########## 'variables' block #############
# Definition of CMake variables from CMakeToolchain.variables values

# Variables
set(CMAKE_PROJECT_VERSION "" CACHE STRING "Variable CMAKE_PROJECT_VERSION conan-toolchain defined")
set(ODR_TEST OFF CACHE BOOL "Variable ODR_TEST conan-toolchain defined")
set(ODR_WITH_LIBMAGIC "False" CACHE STRING "Variable ODR_WITH_LIBMAGIC conan-toolchain defined")
set(ODR_WITH_HTTP_SERVER "True" CACHE STRING "Variable ODR_WITH_HTTP_SERVER conan-toolchain defined")
set(ODR_CLI "True" CACHE STRING "Variable ODR_CLI conan-toolchain defined")
set(ODR_PYTHON "False" CACHE STRING "Variable ODR_PYTHON conan-toolchain defined")
set(ODR_JNI "False" CACHE STRING "Variable ODR_JNI conan-toolchain defined")
set(ODR_APPLE "False" CACHE STRING "Variable ODR_APPLE conan-toolchain defined")
set(ODR_WASM "False" CACHE STRING "Variable ODR_WASM conan-toolchain defined")
set(ODR_BUNDLE_ASSETS "False" CACHE STRING "Variable ODR_BUNDLE_ASSETS conan-toolchain defined")
# Variables  per configuration



########## 'preprocessor' block #############
# Preprocessor definitions from CMakeToolchain.preprocessor_definitions values

# Preprocessor definitions per configuration



if(CMAKE_POLICY_DEFAULT_CMP0091)  # Avoid unused and not-initialized warnings
endif()
