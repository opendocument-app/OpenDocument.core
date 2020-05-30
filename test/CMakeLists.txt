include(FetchContent)

FetchContent_Populate(
        odr-test-data
        GIT_REPOSITORY https://github.com/opendocument-app/OpenDocument.test.git
        GIT_TAG 5eebbd3
        SOURCE_DIR input/odr-test-data
)

find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

enable_testing()
add_executable(odr_test
        DocumentTest.cpp
        OoxmlCryptoTest.cpp
        PathTest.cpp
        TableCursorTest.cpp
        TablePositionTest.cpp
        TableRangeTest.cpp
        TranslateTest.cpp
        ZipStorageTest.cpp
        )
target_include_directories(odr_test
        PRIVATE
        ..
        )
target_link_libraries(odr_test
        PRIVATE
        tinyxml2
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
