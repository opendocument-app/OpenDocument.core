include("FetchContent")

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.8.1
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

enable_testing()
file(GLOB TEST_SOURCES "*.cpp")
add_executable(odrtests ${TEST_SOURCES})
target_link_libraries(odrtests PRIVATE gtest_main gtest glog tinyxml2 odrlib)
if (NOT ODR_DISABLE_CRYPTO AND CryptoPP_FOUND)
    target_link_libraries(odrtests PRIVATE cryptopp)
    target_compile_definitions(odrtests PRIVATE ODR_CRYPTO)
endif()
target_include_directories(odrtests PRIVATE "../src")
gtest_add_tests(TARGET odrtests)
