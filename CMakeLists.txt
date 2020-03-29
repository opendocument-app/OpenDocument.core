cmake_minimum_required(VERSION 3.12.4)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 14)

option(ODR_TEST "enable tests" "ON")

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# use ccache if available
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif (CCACHE_FOUND)

add_subdirectory(3rdparty)

add_subdirectory(access)
add_subdirectory(common)
add_subdirectory(crypto)
add_subdirectory(odf)
add_subdirectory(ooxml)
add_subdirectory(svm)

add_subdirectory(odr)
add_subdirectory(cli)

if (ODR_TEST)
    add_subdirectory(test)
endif ()
