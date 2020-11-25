find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

enable_testing()
add_executable(odr_test
        DataDrivenTests.cpp
        DocumentTest.cpp
        OoxmlCryptoTest.cpp
        OpenDocumentTextTest.cpp
        PathTest.cpp
        TableCursorTest.cpp
        TablePositionTest.cpp
        TableRangeTest.cpp
        ZipStorageTest.cpp
        )
target_include_directories(odr_test
        PRIVATE
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
