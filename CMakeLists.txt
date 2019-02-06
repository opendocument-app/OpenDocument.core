cmake_minimum_required(VERSION 3.10)
project(opendocument C CXX)
set(CMAKE_CXX_STANDARD 14)

find_package(glog 0.3.5 REQUIRED NO_MODULE)

option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/miniz")

add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE "lib/rapidxml")

file(GLOB SOURCES "src/*.cpp")
add_library(opendocument STATIC ${SOURCES})
target_compile_definitions(opendocument PRIVATE USE_MINIZ=1)
target_include_directories(opendocument PUBLIC "include")
target_link_libraries(opendocument miniz rapidxml)
set_property(TARGET opendocument PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")
