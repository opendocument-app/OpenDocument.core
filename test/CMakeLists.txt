find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

enable_testing()
file(GLOB TEST_SOURCES "*.cpp")
add_executable(odrtests ${TEST_SOURCES})
target_link_libraries(odrtests gtest_main gtest glog tinyxml2 cryptopp odrlib)
target_include_directories(odrtests PRIVATE "../src")
gtest_add_tests(TARGET odrtests)
