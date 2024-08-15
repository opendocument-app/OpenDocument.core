cmake_minimum_required(VERSION 3.15)

project(odr LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(ODR_TEST "enable tests" OFF)
option(ODR_CLANG_TIDY "Run clang-tidy static analysis" OFF)
option(WITH_PDF2HTMLEX "Build with pdf2htmlEX" ON)

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
        "src/odr/internal/cfb/cfb_file.cpp"
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
        "src/odr/internal/pdf/pdf_file_object.cpp"
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
        "src/odr/internal/zip/zip_file.cpp"
        "src/odr/internal/zip/zip_util.cpp"
)
if(WITH_PDF2HTMLEX)
        LIST(APPEND ODR_SOURCE_FILES
            "src/odr/internal/html/pdf2htmlEX_wrapper.cpp"
    )
endif(WITH_PDF2HTMLEX)

add_library(odr ${ODR_SOURCE_FILES})
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

if(WITH_PDF2HTMLEX)
    target_compile_definitions(odr PRIVATE "WITH_PDF2HTMLEX=1")
    find_package(pdf2htmlEX REQUIRED)
    target_link_libraries(odr PRIVATE pdf2htmlex::pdf2htmlex)
endif(WITH_PDF2HTMLEX)

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
        DIRECTORY src/
        DESTINATION include/
        FILES_MATCHING PATTERN "*.hpp"
)
install(
        TARGETS odr meta translate back_translate
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
