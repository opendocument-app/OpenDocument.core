find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

set(ODR_TEST_DATA_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/data)
configure_file(TestMeta.cpp.in TestMeta.cpp)

enable_testing()
add_executable(odr_test
        DocumentTest.cpp
        OoxmlCryptoTest.cpp
        PathTest.cpp
        TableCursorTest.cpp
        TablePositionTest.cpp
        TableRangeTest.cpp
        DataDrivenTests.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/TestMeta.cpp
        ZipStorageTest.cpp
        )
target_include_directories(odr_test
        PRIVATE
        ..
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

        odr_access
        odr_common
        odr_crypto
        odr_odf
        odr_ooxml
        odr_svm

        odr-static
        )
gtest_add_tests(TARGET odr_test)
