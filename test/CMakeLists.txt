find_package(GTest REQUIRED)

include(GoogleTest REQUIRED)

set(ODR_TEST_DATA_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/data")
configure_file("src/test_constants.cpp.in" "src/test_constants.cpp")

enable_testing()

if(WITH_PDF2HTMLEX)
    LIST(APPEND ODR_TEST_SOURCE_FILES
            "src/pdf2htmlEX_wrapper_test.cpp"
    )
endif(WITH_PDF2HTMLEX)
add_executable(odr_test
        "src/test_util.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/src/test_constants.cpp"

        "src/document_path_test.cpp"
        "src/document_test.cpp"
        "src/file_test.cpp"
        "src/open_document_reader_test.cpp"
        "src/html_output_test.cpp"
        "src/quantity_test.cpp"

        "src/internal/magic_test.cpp"

        "src/internal/cfb/cfb_archive_test.cpp"

        "src/internal/common/path_test.cpp"
        "src/internal/common/table_cursor_test.cpp"
        "src/internal/common/table_position_test.cpp"
        "src/internal/common/table_range_test.cpp"

        "src/internal/csv/csv_file_test.cpp"
        "src/internal/csv/csv_test.cpp"

        "src/internal/ooxml/ooxml_crypto_test.cpp"

        "src/internal/pdf/pdf_document_parser.cpp"
        "src/internal/pdf/pdf_file_parser.cpp"

        "src/internal/svm/svm_test.cpp"

        "src/internal/text/text_file_test.cpp"

        "src/internal/util/map_util_test.cpp"
        "src/internal/util/xml_util_test.cpp"

        "src/internal/zip/miniz_test.cpp"
        "src/internal/zip/zip_archive_test.cpp"

        ${ODR_TEST_SOURCE_FILES}
)
target_include_directories(odr_test
        PRIVATE
        "src"
        "../src"
)
target_link_libraries(odr_test
        PRIVATE
        pugixml::pugixml
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        vincentlaucsb-csv-parser::vincentlaucsb-csv-parser
        uchardet::uchardet
        GTest::gtest
        GTest::gtest_main

        odr
)

if(WITH_PDF2HTMLEX)
    target_link_libraries(odr_test PRIVATE pdf2htmlex::pdf2htmlex)
endif(WITH_PDF2HTMLEX)
gtest_add_tests(TARGET odr_test)
