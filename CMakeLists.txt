cmake_minimum_required(VERSION 3.12.4)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

option(ODR_DISABLE_CRYPTO "disable crypto" "OFF")

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# use ccache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

include("FetchContent")

FetchContent_Declare(
        tinyxml2
        GIT_REPOSITORY https://github.com/tomtasche/tinyxml2.git
        GIT_TAG 5effa6d
)
FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/andiwand/miniz.git
        GIT_TAG cd99aff
)
FetchContent_Declare(
        glog
        GIT_REPOSITORY https://github.com/tomtasche/glog.git
        GIT_TAG fb5b382
)
FetchContent_Declare(
        cryptopp
        GIT_REPOSITORY https://github.com/TomTasche/cryptopp.ios.git
        GIT_TAG 8c373c0
)

FetchContent_GetProperties(tinyxml2)
if(NOT tinyxml2_POPULATED)
    FetchContent_Populate(tinyxml2)
    option(BUILD_SHARED_LIBS "OFF")
    option(BUILD_TESTS "OFF")
    add_subdirectory(${tinyxml2_SOURCE_DIR} ${tinyxml2_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
FetchContent_GetProperties(miniz)
if(NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
    add_subdirectory(${miniz_SOURCE_DIR} ${miniz_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
FetchContent_GetProperties(glog)
if(NOT glog_POPULATED)
    FetchContent_Populate(glog)
    add_subdirectory(${glog_SOURCE_DIR} ${glog_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(odrlib STATIC ${SOURCES})
target_include_directories(odrlib PUBLIC "include")

target_link_libraries(odrlib PRIVATE glog miniz tinyxml2)

# miniz defines a lot of macros without that
target_compile_definitions(odrlib PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

if(NOT ODR_DISABLE_CRYPTO)
    FetchContent_GetProperties(cryptopp)
    if(NOT cryptopp_POPULATED)
        FetchContent_Populate(cryptopp)
        add_subdirectory(${cryptopp_SOURCE_DIR} ${cryptopp_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    target_link_libraries(odrlib PRIVATE cryptopp-static)
    target_compile_definitions(odrlib PRIVATE ODR_CRYPTO)
endif()

set_property(TARGET odrlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")

add_subdirectory("main")
