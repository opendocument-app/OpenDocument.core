find_package(GTest REQUIRED)

include(GoogleTest REQUIRED)

# The inputs and the reference output, pinned in `data.cmake` and fetched by
# `../cmake/test_data.cmake` — they used to be git submodules, see that file.
#
# Opt in: `ODR_TEST_FETCH_DATA` is off by default, because building `odr_test`
# does not require the data and several gigabytes is not something to download
# because someone turned tests on.
set(ODR_TEST_DATA_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../cmake/test_data.cmake")
if (ODR_TEST_FETCH_DATA)
    include("${ODR_TEST_DATA_SCRIPT}")
endif ()
add_custom_target(update_test_data
        COMMAND "${CMAKE_COMMAND}" -DODR_TEST_DATA_UPDATE=ON -P "${ODR_TEST_DATA_SCRIPT}"
        COMMENT "Moving test/data onto the revisions pinned in test/data.cmake")

set(ODR_TEST_DATA_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/data")
configure_file("src/test_info.cpp.in" "src/test_info.cpp")

enable_testing()
add_executable(odr_test
        "src/test_main.cpp"

        "src/test_util.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/src/test_info.cpp"

        "src/document_list_test.cpp"
        "src/document_path_test.cpp"
        "src/document_test.cpp"
        "src/file_test.cpp"
        "src/html_output_test.cpp"
        "src/html_test.cpp"
        "src/logger_test.cpp"
        "src/odr_test.cpp"
        "src/quantity_test.cpp"
        "src/table_position_test.cpp"

        "src/internal/html/common_test.cpp"
        "src/internal/html/document_style_test.cpp"
        "src/internal/html/image_file_test.cpp"
        "src/internal/html/media_file_test.cpp"

        "src/internal/magic_test.cpp"

        "src/internal/cfb/cfb_archive_test.cpp"

        "src/internal/common/filesystem_test.cpp"
        "src/internal/common/list_numbering_test.cpp"
        "src/internal/common/path_test.cpp"
        "src/internal/common/table_cursor_test.cpp"
        "src/internal/common/table_range_test.cpp"

        "src/internal/crypto/crypto_util_test.cpp"

        "src/internal/csv/csv_file_test.cpp"
        "src/internal/encoding/text_encoding_test.cpp"
        "src/internal/svg/svg_file_test.cpp"
        "src/internal/xml/xml_file_test.cpp"

        "src/internal/markdown/markdown_file_test.cpp"

        "src/internal/iwork/iwork_archive_test.cpp"
        "src/internal/iwork/iwork_protobuf_test.cpp"
        "src/internal/iwork/iwork_snappy_test.cpp"
        "src/internal/iwork/keynote_test.cpp"
        "src/internal/iwork/pages_test.cpp"

        "src/internal/rtf/rtf_document_test.cpp"
        "src/internal/rtf/rtf_tokenizer_test.cpp"

        "src/internal/odf/odf_flat_file_test.cpp"
        "src/internal/odf/odf_table_test.cpp"

        "src/internal/oldms/doc_test.cpp"
        "src/internal/oldms/encryption_test.cpp"
        "src/internal/oldms/ppt_test.cpp"
        "src/internal/oldms/xls_test.cpp"

        "src/internal/ooxml/ooxml_crypto_test.cpp"
        "src/internal/ooxml/ooxml_text_style_test.cpp"

        "src/internal/pdf/pdf_cid.cpp"
        "src/internal/pdf/pdf_cmap.cpp"
        "src/internal/pdf/pdf_color.cpp"
        "src/internal/pdf/pdf_document_parser.cpp"
        "src/internal/pdf/pdf_encoding.cpp"
        "src/internal/pdf/pdf_encryption.cpp"
        "src/internal/pdf/pdf_file.cpp"
        "src/internal/pdf/pdf_file_object.cpp"
        "src/internal/pdf/pdf_file_parser.cpp"
        "src/internal/pdf/pdf_filter.cpp"
        "src/internal/pdf/pdf_font.cpp"
        "src/internal/pdf/pdf_function.cpp"
        "src/internal/pdf/pdf_image.cpp"
        "src/internal/pdf/pdf_jbig2.cpp"
        "src/internal/pdf/pdf_jpx.cpp"
        "src/internal/util/math_util_test.cpp"
        "src/internal/pdf/pdf_object.cpp"
        "src/internal/pdf/pdf_object_parser.cpp"
        "src/internal/pdf/pdf_page_extractor.cpp"
        "src/internal/pdf/pdf_shading.cpp"
        "src/internal/pdf/pdf_test_file_builder.cpp"

        "src/internal/font/cff_font.cpp"
        "src/internal/font/type1_charstring.cpp"
        "src/internal/font/type1_crypt.cpp"
        "src/internal/font/type1_font.cpp"
        "src/internal/font/sfnt_font.cpp"
        "src/internal/font/sfnt_transform.cpp"
        "src/internal/font/font_file.cpp"

        "src/internal/svm/svm_test.cpp"

        "src/internal/text/text_file_test.cpp"

        "src/internal/util/map_util_test.cpp"
        "src/internal/util/number_util_test.cpp"
        "src/internal/util/stream_util_test.cpp"
        "src/internal/util/string_util_test.cpp"
        "src/internal/util/xml_util_test.cpp"

        "src/internal/zip/miniz_test.cpp"
        "src/internal/zip/zip_archive_test.cpp"
)
if (ODR_WITH_HTTP_SERVER)
    target_sources(odr_test
            PRIVATE
            "src/http_server_test.cpp"
    )
    # a client to make a request with, so the server can be tested serving one
    find_package(httplib REQUIRED)
    target_link_libraries(odr_test PRIVATE httplib::httplib)
endif ()
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
        uchardet::uchardet
        GTest::gtest

        odr
)

gtest_add_tests(TARGET odr_test)
