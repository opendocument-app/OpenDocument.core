cmake_minimum_required(VERSION 3.15)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif ()

project(odr LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(ODR_TEST "enable tests" OFF)
option(ODR_TEST_FETCH_DATA "Fetch the test data pinned in test/data.cmake" OFF)
option(ODR_CLI "enable command line interface" ON)
option(ODR_WITH_HTTP_SERVER "Build the HTTP server (requires cpp-httplib)" ON)
option(ODR_CLANG_TIDY "Run clang-tidy static analysis" OFF)
# Removed, and accepted only so a consumer still passing it keeps configuring.
# `odr::mimetype` is our own detection now: it reads inside zip and cfb
# containers, which libmagic cannot, and it costs neither a dependency nor the
# 8 MB `magic.mgc` database every consumer had to ship and point us at.
option(ODR_WITH_LIBMAGIC "Removed, does nothing (deprecated)" OFF)
option(ODR_BUNDLE_ASSETS "Removed, does nothing (deprecated)" OFF)
option(ODR_PYTHON "Build Python bindings" OFF)
option(ODR_JNI "Build JNI bindings" OFF)
option(ODR_APPLE "Build Objective-C bindings as a framework" OFF)
option(ODR_WASM "Build WebAssembly bindings" OFF)

include(GNUInstallDirs)

# TODO defining global compiler flags seems to be bad practice with conan
# TODO consider using conan profiles
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR
        CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using clang or gcc

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    # debugging
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
    # benchmarking
    #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # using Visual Studio C++

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif ()

# MSVC reads sources in the system codepage by default; force UTF-8 so non-ASCII
# literals (e.g. the PDFDocEncoding table) compile (C2015) and encode correctly.
# GCC/Clang already default to UTF-8.
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

find_package(pugixml REQUIRED)
find_package(md4c REQUIRED)
find_package(miniz REQUIRED)
find_package(cryptopp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenJPEG REQUIRED)
find_package(uchardet REQUIRED)
find_package(utf8cpp REQUIRED)

set(PRE_CONFIGURE_FILE "src/odr/internal/git_info.cpp.in")
set(POST_CONFIGURE_FILE "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/git_info.cpp")
# An explicitly injected `GIT_HEAD_SHA1` wins over the working tree, which is
# what lets a release identify itself by its tag. Without that, the Apple
# release would be circular: `Package.swift` must carry the checksum of an
# artifact built from the commit that contains `Package.swift`, and baking the
# working-tree sha into the binary makes the checksum change every time the
# checksum is written. Identifying by tag breaks the loop, and is the more
# useful answer for a released artifact anyway.
if (EXISTS "${PROJECT_SOURCE_DIR}/.git" AND NOT DEFINED GIT_HEAD_SHA1)
    set(ODR_GIT_WATCHED TRUE)
    include("cmake/git_watcher.cmake")
else ()
    set(ODR_GIT_WATCHED FALSE)
    if (NOT DEFINED GIT_HEAD_SHA1)
        set(GIT_HEAD_SHA1 "unknown")
    endif ()
    if (NOT DEFINED GIT_IS_DIRTY)
        set(GIT_IS_DIRTY "false")
    endif ()
    configure_file("${PRE_CONFIGURE_FILE}" "${POST_CONFIGURE_FILE}" @ONLY)
endif ()

set(ODR_SOURCE_FILES
        "src/odr/archive.cpp"
        "src/odr/document.cpp"
        "src/odr/document_element.cpp"
        "src/odr/document_path.cpp"
        "src/odr/exceptions.cpp"
        "src/odr/file.cpp"
        "src/odr/filesystem.cpp"
        "src/odr/global_params.cpp"
        "src/odr/html.cpp"
        "src/odr/logger.cpp"
        "src/odr/odr.cpp"
        "src/odr/quantity.cpp"
        "src/odr/style.cpp"
        "src/odr/table_dimension.cpp"
        "src/odr/table_position.cpp"

        "src/odr/internal/encoding/detect.cpp"
        "src/odr/internal/encoding/encoding_data.cpp"
        "src/odr/internal/encoding/text_encoding_table.cpp"
        "src/odr/internal/encoding/transcode.cpp"
        "src/odr/internal/file_type_table.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/git_info.cpp"
        "src/odr/internal/magic.cpp"
        "src/odr/internal/open_strategy.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/project_info.cpp"

        "src/odr/internal/cfb/cfb_archive.cpp"
        "src/odr/internal/cfb/cfb_file.cpp"
        "src/odr/internal/cfb/cfb_impl.cpp"
        "src/odr/internal/cfb/cfb_util.cpp"

        "src/odr/internal/common/document.cpp"
        "src/odr/internal/common/file.cpp"
        "src/odr/internal/common/filesystem.cpp"
        "src/odr/internal/common/image_file.cpp"
        "src/odr/internal/common/list_numbering.cpp"
        "src/odr/internal/common/media_file.cpp"
        "src/odr/internal/common/path.cpp"
        "src/odr/internal/common/random.cpp"
        "src/odr/internal/common/style.cpp"
        "src/odr/internal/common/table_cursor.cpp"
        "src/odr/internal/common/table_range.cpp"
        "src/odr/internal/common/temporary_file.cpp"

        "src/odr/internal/crypto/crypto_argon2.cpp"
        "src/odr/internal/crypto/crypto_util.cpp"

        "src/odr/internal/csv/csv_document.cpp"
        "src/odr/internal/csv/csv_file.cpp"
        "src/odr/internal/csv/csv_util.cpp"

        "src/odr/internal/html/common.cpp"
        "src/odr/internal/html/document.cpp"
        "src/odr/internal/html/document_style.cpp"
        "src/odr/internal/html/document_element.cpp"
        "src/odr/internal/html/filesystem.cpp"
        "src/odr/internal/html/font_file.cpp"
        "src/odr/internal/html/frontend.cpp"
        "src/odr/internal/html/html_service.cpp"
        "src/odr/internal/html/html_writer.cpp"
        "src/odr/internal/html/image_file.cpp"
        "src/odr/internal/html/media_file.cpp"
        "src/odr/internal/html/pdf_file.cpp"
        "src/odr/internal/html/text_file.cpp"
        "src/odr/internal/html/xml_file.cpp"

        "src/odr/internal/iwork/iwork_archive.cpp"
        "src/odr/internal/iwork/iwork_document.cpp"
        "src/odr/internal/iwork/iwork_element_registry.cpp"
        "src/odr/internal/iwork/iwork_file.cpp"
        "src/odr/internal/iwork/iwork_parser.cpp"
        "src/odr/internal/iwork/iwork_protobuf.cpp"
        "src/odr/internal/iwork/iwork_snappy.cpp"
        "src/odr/internal/iwork/iwork_table.cpp"
        "src/odr/internal/iwork/iwork_text.cpp"

        "src/odr/internal/json/json_file.cpp"
        "src/odr/internal/json/json_util.cpp"

        "src/odr/internal/markdown/markdown_document.cpp"
        "src/odr/internal/markdown/markdown_element_registry.cpp"
        "src/odr/internal/markdown/markdown_file.cpp"
        "src/odr/internal/markdown/markdown_parser.cpp"
        "src/odr/internal/markdown/markdown_style.cpp"

        "src/odr/internal/odf/odf_crypto.cpp"
        "src/odr/internal/odf/odf_document.cpp"
        "src/odr/internal/odf/odf_element_registry.cpp"
        "src/odr/internal/odf/odf_file.cpp"
        "src/odr/internal/odf/odf_flat_file.cpp"
        "src/odr/internal/odf/odf_list.cpp"
        "src/odr/internal/odf/odf_manifest.cpp"
        "src/odr/internal/odf/odf_meta.cpp"
        "src/odr/internal/odf/odf_parser.cpp"
        "src/odr/internal/odf/odf_style.cpp"
        "src/odr/internal/odf/odf_table.cpp"

        "src/odr/internal/oldms/text/doc_document.cpp"
        "src/odr/internal/oldms/text/doc_element_registry.cpp"
        "src/odr/internal/oldms/text/doc_helper.cpp"
        "src/odr/internal/oldms/text/doc_io.cpp"
        "src/odr/internal/oldms/text/doc_parser.cpp"
        "src/odr/internal/oldms/text/doc_style.cpp"
        "src/odr/internal/oldms/spreadsheet/xls_document.cpp"
        "src/odr/internal/oldms/spreadsheet/xls_element_registry.cpp"
        "src/odr/internal/oldms/spreadsheet/xls_io.cpp"
        "src/odr/internal/oldms/spreadsheet/xls_parser.cpp"
        "src/odr/internal/oldms/spreadsheet/xls_style.cpp"
        "src/odr/internal/oldms/presentation/ppt_document.cpp"
        "src/odr/internal/oldms/presentation/ppt_element_registry.cpp"
        "src/odr/internal/oldms/presentation/ppt_io.cpp"
        "src/odr/internal/oldms/presentation/ppt_parser.cpp"
        "src/odr/internal/oldms/presentation/ppt_style.cpp"
        "src/odr/internal/oldms/oldms_file.cpp"

        "src/odr/internal/ooxml/presentation/ooxml_presentation_document.cpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_element_registry.cpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_parser.cpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_style.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element_registry.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_document.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_element_registry.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_list.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_parser.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_style.cpp"
        "src/odr/internal/ooxml/ooxml_crypto.cpp"
        "src/odr/internal/ooxml/ooxml_file.cpp"
        "src/odr/internal/ooxml/ooxml_meta.cpp"
        "src/odr/internal/ooxml/ooxml_util.cpp"

        "src/odr/internal/pdf/pdf_afm.cpp"
        "src/odr/internal/pdf/pdf_afm_data.cpp"
        "src/odr/internal/pdf/pdf_cid.cpp"
        "src/odr/internal/pdf/pdf_cid_data.cpp"
        "src/odr/internal/pdf/pdf_cmap.cpp"
        "src/odr/internal/pdf/pdf_cmap_parser.cpp"
        "src/odr/internal/pdf/pdf_color.cpp"
        "src/odr/internal/pdf/pdf_document.cpp"
        "src/odr/internal/pdf/pdf_document_parser.cpp"
        "src/odr/internal/pdf/pdf_encoding.cpp"
        "src/odr/internal/pdf/pdf_encoding_data.cpp"
        "src/odr/internal/pdf/pdf_encryption.cpp"
        "src/odr/internal/pdf/pdf_file.cpp"
        "src/odr/internal/pdf/pdf_file_object.cpp"
        "src/odr/internal/pdf/pdf_file_parser.cpp"
        "src/odr/internal/pdf/pdf_filter.cpp"
        "src/odr/internal/pdf/pdf_function.cpp"
        "src/odr/internal/pdf/pdf_image.cpp"
        "src/odr/internal/pdf/pdf_jbig2.cpp"
        "src/odr/internal/pdf/pdf_jpx.cpp"
        "src/odr/internal/pdf/pdf_graphics_operator_parser.cpp"
        "src/odr/internal/pdf/pdf_graphics_state.cpp"
        "src/odr/internal/pdf/pdf_object.cpp"
        "src/odr/internal/pdf/pdf_object_parser.cpp"
        "src/odr/internal/pdf/pdf_page_extractor.cpp"
        "src/odr/internal/pdf/pdf_shading.cpp"

        "src/odr/internal/font/cff_builder.cpp"
        "src/odr/internal/font/cff_font.cpp"
        "src/odr/internal/font/cff_standard_strings.cpp"
        "src/odr/internal/font/cff_transform.cpp"
        "src/odr/internal/font/type1_charstring.cpp"
        "src/odr/internal/font/type1_crypt.cpp"
        "src/odr/internal/font/type1_font.cpp"
        "src/odr/internal/font/type1_transform.cpp"
        "src/odr/internal/font/sfnt_font.cpp"
        "src/odr/internal/font/sfnt_transform.cpp"
        "src/odr/internal/font/font_file.cpp"

        "src/odr/internal/rtf/rtf_document.cpp"
        "src/odr/internal/rtf/rtf_element_registry.cpp"
        "src/odr/internal/rtf/rtf_file.cpp"
        "src/odr/internal/rtf/rtf_parser.cpp"
        "src/odr/internal/rtf/rtf_state.cpp"
        "src/odr/internal/rtf/rtf_tokenizer.cpp"

        "src/odr/internal/svg/svg_file.cpp"

        "src/odr/internal/svm/svm_file.cpp"
        "src/odr/internal/svm/svm_format.cpp"
        "src/odr/internal/svm/svm_to_svg.cpp"

        "src/odr/internal/text/text_file.cpp"

        "src/odr/internal/util/byte_util.cpp"
        "src/odr/internal/util/byte_stream_util.cpp"
        "src/odr/internal/util/byte_string.cpp"
        "src/odr/internal/util/document_util.cpp"
        "src/odr/internal/util/file_util.cpp"
        "src/odr/internal/util/hash_util.cpp"
        "src/odr/internal/util/number_util.cpp"
        "src/odr/internal/util/odr_meta_util.cpp"
        "src/odr/internal/util/stream_util.cpp"
        "src/odr/internal/util/string_util.cpp"
        "src/odr/internal/util/xml_util.cpp"

        "src/odr/internal/xml/xml_file.cpp"

        "src/odr/internal/zip/zip_archive.cpp"
        "src/odr/internal/zip/zip_exceptions.cpp"
        "src/odr/internal/zip/zip_file.cpp"
        "src/odr/internal/zip/zip_util.cpp"
)

add_library(odr ${ODR_SOURCE_FILES})
set_target_properties(odr PROPERTIES OUTPUT_NAME odr)
target_include_directories(odr
        PUBLIC
        src
        ${CMAKE_CURRENT_BINARY_DIR}/src
)
target_link_libraries(odr
        PRIVATE
        pugixml::pugixml
        md4c::md4c
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        openjp2
        uchardet::uchardet
        utf8::cpp
)

if (ODR_WITH_HTTP_SERVER)
    find_package(httplib REQUIRED)
    target_sources(odr
            PRIVATE
            "src/odr/http_server.cpp"
    )
    target_link_libraries(odr
            PRIVATE
            httplib::httplib
    )
    target_compile_definitions(odr
            PRIVATE
            ODR_WITH_HTTP_SERVER
    )
endif ()
if (ODR_WITH_LIBMAGIC)
    message(DEPRECATION
            "ODR_WITH_LIBMAGIC no longer does anything: libmagic is gone and "
            "`odr::mimetype` is our own detection. Drop the option; "
            "`GlobalParams::libmagic_database_path` is inert too.")
endif ()
if (ODR_BUNDLE_ASSETS)
    message(DEPRECATION
            "ODR_BUNDLE_ASSETS no longer does anything: the css and js are "
            "part of the library and there is nothing left to ship. Drop the "
            "option; `GlobalParams::odr_core_data_path` is inert too.")
endif ()

configure_file("src/odr/internal/project_info.cpp.in" "src/odr/internal/project_info.cpp")

# `check_git` only exists when the watcher above ran
if (ODR_GIT_WATCHED)
    add_dependencies(odr check_git)
endif ()

if (ODR_CLI)
    add_subdirectory("cli")
endif ()

if (ODR_PYTHON)
    add_subdirectory("python")
endif ()

if (ODR_JNI)
    add_subdirectory("jni")
endif ()

if (ODR_APPLE)
    add_subdirectory("apple")
endif ()

if (ODR_WASM)
    add_subdirectory("wasm")
endif ()

if (ODR_TEST)
    add_subdirectory("test")
endif ()

if (ODR_CLANG_TIDY)
    add_subdirectory("static_analysis/clang-tidy")
endif ()

if (NOT ODR_WITH_HTTP_SERVER)
    # Don't ship the public header for an API that wasn't built.
    set(ODR_HEADER_EXCLUDE PATTERN "http_server.hpp" EXCLUDE)
endif ()
install(
        DIRECTORY src/ "${CMAKE_CURRENT_BINARY_DIR}/src/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.hpp"
        ${ODR_HEADER_EXCLUDE}
)
set(ODR_INSTALL_TARGETS odr)
if (ODR_CLI)
    list(APPEND ODR_INSTALL_TARGETS meta translate back_translate)
    if (ODR_WITH_HTTP_SERVER)
        list(APPEND ODR_INSTALL_TARGETS server)
    endif ()
endif ()
install(
        TARGETS ${ODR_INSTALL_TARGETS}
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
