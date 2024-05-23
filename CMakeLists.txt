cmake_minimum_required(VERSION 3.12.4)

project(odr LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ODR_TEST "enable tests" ON)
option(ODR_CLANG_TIDY "Run clang-tidy static analysis" OFF)

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

find_package(pugixml REQUIRED)
find_package(miniz REQUIRED)
find_package(cryptopp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(vincentlaucsb-csv-parser REQUIRED)
find_package(uchardet REQUIRED)
find_package(utf8cpp REQUIRED)

configure_file("src/odr/internal/project_info.cpp.in" "src/odr/internal/project_info.cpp")

set(PRE_CONFIGURE_FILE "src/odr/internal/git_info.cpp.in")
set(POST_CONFIGURE_FILE "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/git_info.cpp")
if (EXISTS "${PROJECT_SOURCE_DIR}/.git")
    include("cmake/git_watcher.cmake")
else ()
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
        "src/odr/html.cpp"
        "src/odr/open_document_reader.cpp"
        "src/odr/quantity.cpp"
        "src/odr/style.cpp"

        "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/git_info.cpp"
        "src/odr/internal/magic.cpp"
        "src/odr/internal/open_strategy.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/src/odr/internal/project_info.cpp"
        "src/odr/internal/resource.cpp"
        "src/odr/internal/resource_data.cpp"

        "src/odr/internal/cfb/cfb_archive.cpp"
        "src/odr/internal/cfb/cfb_impl.cpp"
        "src/odr/internal/cfb/cfb_util.cpp"

        "src/odr/internal/common/document.cpp"
        "src/odr/internal/common/document_element.cpp"
        "src/odr/internal/common/file.cpp"
        "src/odr/internal/common/filesystem.cpp"
        "src/odr/internal/common/image_file.cpp"
        "src/odr/internal/common/path.cpp"
        "src/odr/internal/common/random.cpp"
        "src/odr/internal/common/style.cpp"
        "src/odr/internal/common/table_cursor.cpp"
        "src/odr/internal/common/table_position.cpp"
        "src/odr/internal/common/table_range.cpp"
        "src/odr/internal/common/temporary_file.cpp"

        "src/odr/internal/crypto/crypto_util.cpp"

        "src/odr/internal/csv/csv_file.cpp"
        "src/odr/internal/csv/csv_util.cpp"

        "src/odr/internal/html/common.cpp"
        "src/odr/internal/html/document.cpp"
        "src/odr/internal/html/document_style.cpp"
        "src/odr/internal/html/document_element.cpp"
        "src/odr/internal/html/filesystem.cpp"
        "src/odr/internal/html/html_writer.cpp"
        "src/odr/internal/html/image_file.cpp"
        "src/odr/internal/html/pdf_file.cpp"
        "src/odr/internal/html/text_file.cpp"

        "src/odr/internal/json/json_file.cpp"
        "src/odr/internal/json/json_util.cpp"

        "src/odr/internal/odf/odf_crypto.cpp"
        "src/odr/internal/odf/odf_document.cpp"
        "src/odr/internal/odf/odf_element.cpp"
        "src/odr/internal/odf/odf_file.cpp"
        "src/odr/internal/odf/odf_manifest.cpp"
        "src/odr/internal/odf/odf_meta.cpp"
        "src/odr/internal/odf/odf_parser.cpp"
        "src/odr/internal/odf/odf_spreadsheet.cpp"
        "src/odr/internal/odf/odf_style.cpp"

        "src/odr/internal/oldms/oldms_file.cpp"

        "src/odr/internal/ooxml/presentation/ooxml_presentation_document.cpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_element.cpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_parser.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.cpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_document.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_element.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_parser.cpp"
        "src/odr/internal/ooxml/text/ooxml_text_style.cpp"
        "src/odr/internal/ooxml/ooxml_crypto.cpp"
        "src/odr/internal/ooxml/ooxml_file.cpp"
        "src/odr/internal/ooxml/ooxml_meta.cpp"
        "src/odr/internal/ooxml/ooxml_util.cpp"

        "src/odr/internal/pdf/pdf_cmap.cpp"
        "src/odr/internal/pdf/pdf_cmap_parser.cpp"
        "src/odr/internal/pdf/pdf_document.cpp"
        "src/odr/internal/pdf/pdf_document_element.cpp"
        "src/odr/internal/pdf/pdf_document_parser.cpp"
        "src/odr/internal/pdf/pdf_file.cpp"
        "src/odr/internal/pdf/pdf_file_parser.cpp"
        "src/odr/internal/pdf/pdf_graphics_operator.cpp"
        "src/odr/internal/pdf/pdf_graphics_operator_parser.cpp"
        "src/odr/internal/pdf/pdf_graphics_state.cpp"
        "src/odr/internal/pdf/pdf_object.cpp"
        "src/odr/internal/pdf/pdf_object_parser.cpp"

        "src/odr/internal/svm/svm_file.cpp"
        "src/odr/internal/svm/svm_format.cpp"
        "src/odr/internal/svm/svm_to_svg.cpp"

        "src/odr/internal/text/text_file.cpp"
        "src/odr/internal/text/text_util.cpp"

        "src/odr/internal/util/byte_util.cpp"
        "src/odr/internal/util/file_util.cpp"
        "src/odr/internal/util/hash_util.cpp"
        "src/odr/internal/util/odr_meta_util.cpp"
        "src/odr/internal/util/stream_util.cpp"
        "src/odr/internal/util/string_util.cpp"
        "src/odr/internal/util/xml_util.cpp"

        "src/odr/internal/zip/zip_archive.cpp"
        "src/odr/internal/zip/zip_exceptions.cpp"
        "src/odr/internal/zip/zip_util.cpp"
)
set(ODR_PUBLIC_HEADER
        "src/odr/html.hpp"
        "src/odr/exceptions.hpp"
        "src/odr/open_document_reader.hpp"
        "src/odr/document_path.hpp"
        "src/odr/style.hpp"
        "src/odr/quantity.hpp"
        "src/odr/document_element.hpp"
        "src/odr/document.hpp"
        "src/odr/archive.hpp"
        "src/odr/filesystem.hpp"
        "src/odr/file.hpp"
)
set(ODR_PRIVATE_HEADER
        "src/odr/internal/git_info.hpp"
        "src/odr/internal/resource_data.hpp"
        "src/odr/internal/open_strategy.hpp"
        "src/odr/internal/resource.hpp"
        "src/odr/internal/project_info.hpp"
        "src/odr/internal/magic.hpp"

        "src/odr/internal/abstract/document_element.hpp"
        "src/odr/internal/abstract/document.hpp"
        "src/odr/internal/abstract/sheet_element.hpp"
        "src/odr/internal/abstract/archive.hpp"
        "src/odr/internal/abstract/filesystem.hpp"
        "src/odr/internal/abstract/file.hpp"

        "src/odr/internal/crypto/crypto_util.hpp"

        "src/odr/internal/odf/odf_manifest.hpp"
        "src/odr/internal/odf/odf_parser.hpp"
        "src/odr/internal/odf/odf_crypto.hpp"
        "src/odr/internal/odf/odf_spreadsheet.hpp"
        "src/odr/internal/odf/odf_file.hpp"
        "src/odr/internal/odf/odf_style.hpp"
        "src/odr/internal/odf/odf_meta.hpp"
        "src/odr/internal/odf/odf_document.hpp"
        "src/odr/internal/odf/odf_element.hpp"

        "src/odr/internal/util/hash_util.hpp"
        "src/odr/internal/util/byte_util.hpp"
        "src/odr/internal/util/string_util.hpp"
        "src/odr/internal/util/map_util.hpp"
        "src/odr/internal/util/odr_meta_util.hpp"
        "src/odr/internal/util/stream_util.hpp"
        "src/odr/internal/util/xml_util.hpp"
        "src/odr/internal/util/file_util.hpp"

        "src/odr/internal/oldms/oldms_file.hpp"

        "src/odr/internal/pdf/pdf_cmap.hpp"
        "src/odr/internal/pdf/pdf_graphics_state.hpp"
        "src/odr/internal/pdf/pdf_file_parser.hpp"
        "src/odr/internal/pdf/pdf_document_parser.hpp"
        "src/odr/internal/pdf/pdf_cmap_parser.hpp"
        "src/odr/internal/pdf/pdf_document_element.hpp"
        "src/odr/internal/pdf/pdf_file_object.hpp"
        "src/odr/internal/pdf/pdf_graphics_operator_parser.hpp"
        "src/odr/internal/pdf/pdf_object.hpp"
        "src/odr/internal/pdf/pdf_document.hpp"
        "src/odr/internal/pdf/pdf_object_parser.hpp"
        "src/odr/internal/pdf/pdf_graphics_operator.hpp"
        "src/odr/internal/pdf/pdf_file.hpp"

        "src/odr/internal/ooxml/ooxml_util.hpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp"
        "src/odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp"
        "src/odr/internal/ooxml/ooxml_file.hpp"
        "src/odr/internal/ooxml/ooxml_crypto.hpp"
        "src/odr/internal/ooxml/ooxml_meta.hpp"
        "src/odr/internal/ooxml/text/ooxml_text_style.hpp"
        "src/odr/internal/ooxml/text/ooxml_text_element.hpp"
        "src/odr/internal/ooxml/text/ooxml_text_parser.hpp"
        "src/odr/internal/ooxml/text/ooxml_text_document.hpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_element.hpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_document.hpp"
        "src/odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp"

        "src/odr/internal/html/document_element.hpp"
        "src/odr/internal/html/document.hpp"
        "src/odr/internal/html/filesystem.hpp"
        "src/odr/internal/html/html_writer.hpp"
        "src/odr/internal/html/document_style.hpp"
        "src/odr/internal/html/common.hpp"
        "src/odr/internal/html/image_file.hpp"
        "src/odr/internal/html/text_file.hpp"
        "src/odr/internal/html/pdf_file.hpp"

        "src/odr/internal/zip/zip_exceptions.hpp"
        "src/odr/internal/zip/zip_util.hpp"
        "src/odr/internal/zip/zip_archive.hpp"

        "src/odr/internal/svm/svm_format.hpp"
        "src/odr/internal/svm/svm_file.hpp"
        "src/odr/internal/svm/svm_to_svg.hpp"

        "src/odr/internal/common/document_element.hpp"
        "src/odr/internal/common/document.hpp"
        "src/odr/internal/common/archive.hpp"
        "src/odr/internal/common/temporary_file.hpp"
        "src/odr/internal/common/filesystem.hpp"
        "src/odr/internal/common/file.hpp"
        "src/odr/internal/common/path.hpp"
        "src/odr/internal/common/random.hpp"
        "src/odr/internal/common/table_range.hpp"
        "src/odr/internal/common/table_position.hpp"
        "src/odr/internal/common/table_cursor.hpp"
        "src/odr/internal/common/image_file.hpp"
        "src/odr/internal/common/style.hpp"

        "src/odr/internal/json/json_file.hpp"
        "src/odr/internal/json/json_util.hpp"

        "src/odr/internal/csv/csv_util.hpp"
        "src/odr/internal/csv/csv_file.hpp"

        "src/odr/internal/text/text_util.hpp"
        "src/odr/internal/text/text_file.hpp"

        "src/odr/internal/cfb/cfb_util.hpp"
        "src/odr/internal/cfb/cfb_impl.hpp"
        "src/odr/internal/cfb/cfb_archive.hpp"
)

add_library(odr ${ODR_SOURCE_FILES})
set_target_properties(odr PROPERTIES PUBLIC_HEADER "${ODR_PUBLIC_HEADER}")
set_target_properties(odr PROPERTIES PRIVATE_HEADER "${ODR_PRIVATE_HEADER}")
set_target_properties(odr PROPERTIES OUTPUT_NAME odr)
target_include_directories(odr
        PUBLIC
        src
)
target_link_libraries(odr
        PRIVATE
        pugixml::pugixml
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        vincentlaucsb-csv-parser::vincentlaucsb-csv-parser
        uchardet::uchardet
        utf8::cpp
)

if (EXISTS "${PROJECT_SOURCE_DIR}/.git")
    add_dependencies(odr check_git)
endif ()

add_subdirectory("cli")

if (ODR_TEST)
    add_subdirectory("test")
endif ()

if (ODR_CLANG_TIDY)
    add_subdirectory("static_analysis/clang-tidy")
endif ()

install(
        TARGETS odr meta translate back_translate
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
