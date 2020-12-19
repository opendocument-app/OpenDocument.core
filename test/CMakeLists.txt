find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

set(ODR_TEST_DATA_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/data)
configure_file(TestConstants.cpp.in TestConstants.cpp)

enable_testing()
add_executable(odr_test
        access/ZipStorageTest.cpp

        common/PathTest.cpp
        common/TableCursorTest.cpp
        common/TablePositionTest.cpp
        common/TableRangeTest.cpp

        ooxml/OoxmlCryptoTest.cpp

        TestMeta.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/TestConstants.cpp
        DocumentTest.cpp
        FileNoExcept.cpp
        FileTest.cpp
        DataDrivenTests.cpp
        )
target_include_directories(odr_test
        PRIVATE
        ..
        ../src
        )
target_link_libraries(odr_test
        PRIVATE
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json
        csv
        gtest_main
        gtest

        odr-static
        )
gtest_add_tests(TARGET odr_test)
