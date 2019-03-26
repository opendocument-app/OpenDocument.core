cmake_minimum_required(VERSION 3.10)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

find_package(OpenSSL COMPONENTS Crypto REQUIRED)
find_package(glog 0.3.5 REQUIRED NO_MODULE)

option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/tinyxml2")
add_subdirectory("lib/miniz")

file(GLOB SOURCES "src/*.cpp")
add_library(odrlib STATIC ${SOURCES})
target_include_directories(odrlib PUBLIC "include")
target_link_libraries(odrlib PUBLIC miniz tinyxml2)
if(OPENSSL_FOUND)
    target_link_libraries(odrlib PRIVATE OpenSSL::Crypto)
    target_compile_definitions(odrlib PRIVATE ODR_CRYPTO)
endif()
set_property(TARGET odrlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")
