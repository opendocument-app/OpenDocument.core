cmake_minimum_required(VERSION 3.10)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

option(ODR_DISABLE_CRYPTO "disable crypto" "OFF")

# tinyxml2 options
option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/tinyxml2")
add_subdirectory("lib/miniz")
add_subdirectory("lib/glog")

file(GLOB SOURCES "src/*.cpp")
add_library(odrlib STATIC ${SOURCES})
target_include_directories(odrlib PUBLIC "include")

target_link_libraries(odrlib PRIVATE glog miniz tinyxml2)

# miniz defines a lot of macros without that
target_compile_definitions(odrlib PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)

if (NOT ODR_DISABLE_CRYPTO)
    add_subdirectory("lib/cryptopp")

    target_link_libraries(odrlib PRIVATE cryptopp-static)
    target_compile_definitions(odrlib PRIVATE ODR_CRYPTO)
endif()

set_property(TARGET odrlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")
