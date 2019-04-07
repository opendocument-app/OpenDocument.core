cmake_minimum_required(VERSION 3.10)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

option(ODR_DISABLE_CRYPTO "disable crypto" "OFF")

option(BUILD_SHARED_LIBS "OFF")
option(BUILD_TESTS "OFF")
add_subdirectory("lib/tinyxml2")
add_subdirectory("lib/miniz")
add_subdirectory("lib/glog")

# TODO: remove
set(CryptoPP_FOUND ON)
add_library(cryptopp STATIC IMPORTED)
set_target_properties(cryptopp PROPERTIES IMPORTED_LOCATION "/home/andreas/Desktop/odr/cryptopp/libcryptopp.a")
target_include_directories(cryptopp INTERFACE "/home/andreas/Desktop/odr")

file(GLOB SOURCES "src/*.cpp")
add_library(odrlib STATIC ${SOURCES})
target_include_directories(odrlib PUBLIC "include")
target_link_libraries(odrlib PUBLIC miniz tinyxml2 glog)
if(NOT ODR_DISABLE_CRYPTO)
    target_link_libraries(odrlib PRIVATE cryptopp-static)
    target_compile_definitions(odrlib PRIVATE ODR_CRYPTO)
endif()
set_property(TARGET odrlib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_subdirectory("test")
