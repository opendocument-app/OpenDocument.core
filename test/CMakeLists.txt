find_package(Threads REQUIRED)
include(GoogleTest REQUIRED)

set(ODR_TEST_DATA_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/data)
configure_file(src/test_constants.cpp.in src/test_constants.cpp)

enable_testing()
add_executable(odr_test
        src/test_util.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/test_constants.cpp

        src/document_test.cpp
        src/file_no_except_test.cpp
        src/file_test.cpp
        src/output_reference_test.cpp

        src/internal/cfb/cfb_archive_test.cpp

        src/internal/common/archive_test.cpp
        src/internal/common/cache_test.cpp
        src/internal/common/path_test.cpp
        src/internal/common/table_cursor_test.cpp
        src/internal/common/table_position_test.cpp
        src/internal/common/table_range_test.cpp

        src/internal/ooxml/ooxml_crypto_test.cpp

        src/internal/svm/svm_test.cpp

        src/internal/zip/miniz_test.cpp
        src/internal/zip/zip_archive_test.cpp
        )
target_include_directories(odr_test
        PRIVATE
        src
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
