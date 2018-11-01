find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

enable_testing()
file(GLOB TEST_SOURCES "*.cpp")
add_executable(opendocumenttests ${TEST_SOURCES})
target_link_libraries(opendocumenttests gtest_main gtest glog::glog opendocument)
target_include_directories(opendocument PUBLIC "../src")
gtest_add_tests(TARGET opendocumenttests)
