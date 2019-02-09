cmake_minimum_required(VERSION 3.10)
project(opendocument C CXX)
set(CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/tinyxml2")
add_subdirectory("lib/miniz")
add_subdirectory("lib/glog")

# workaround for buggy miniz CMakeLists
include_directories("lib/miniz")

file(GLOB SOURCES "src/*.cpp")
add_library(opendocument STATIC ${SOURCES})
target_include_directories(opendocument PUBLIC "src")
target_link_libraries(opendocument miniz tinyxml2 glog)
set_property(TARGET opendocument PROPERTY POSITION_INDEPENDENT_CODE ON)
