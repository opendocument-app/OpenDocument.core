find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

set(ODR_TEST_DATA_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/data)
configure_file(test_constants.cpp.in test_constants.cpp)

enable_testing()
add_executable(odr_test
        test_meta.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/test_constants.cpp
        document_test.cpp
        file_no_except_test.cpp
        file_test.cpp
        data_driven_tests.cpp

        cfb/cfb_archive_test.cpp

        common/cache_test.cpp
        common/path_test.cpp
        common/table_cursor_test.cpp
        common/table_position_test.cpp
        common/table_range_test.cpp

        ooxml/ooxml_crypto_test.cpp

        zip/miniz_test.cpp
        zip/zip_archive_test.cpp
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
