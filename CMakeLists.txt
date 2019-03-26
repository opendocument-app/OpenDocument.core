cmake_minimum_required(VERSION 3.10)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

find_package(glog 0.3.5 REQUIRED NO_MODULE)

option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/tinyxml2")
add_subdirectory("lib/miniz")

file(GLOB SOURCES "src/*.cpp")
add_library(odrlib STATIC ${SOURCES})
target_include_directories(odrlib PUBLIC "include")
target_link_libraries(odrlib miniz tinyxml2)
set_property(TARGET odrlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")
