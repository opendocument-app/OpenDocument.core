cmake_minimum_required(VERSION 3.12.4)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ODR_TEST "enable tests" ON)

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

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

# use ccache if available
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_FOUND}")
    message(STATUS "using ccache ${CCACHE_FOUND}")
endif (CCACHE_FOUND)

include(cmake/conan.cmake)
conan_cmake_configure(GENERATORS cmake)
conan_cmake_autodetect(settings)
conan_cmake_install(PATH_OR_REFERENCE ..
        BUILD missing
        SETTINGS ${settings}
        SETTINGS compiler.cppstd=17)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS NO_OUTPUT_DIRS)

add_library(odr-interface INTERFACE)
target_include_directories(odr-interface INTERFACE include)

add_library(odr-object OBJECT
        src/document.cpp
        src/document_cursor.cpp
        src/document_element.cpp
        src/exceptions.cpp
        src/file.cpp
        src/html.cpp
        src/open_document_reader.cpp
        src/open_strategy.cpp
        src/quantity.cpp
        src/style.cpp

        src/internal/cfb/cfb_archive.cpp
        src/internal/cfb/cfb_impl.cpp
        src/internal/cfb/cfb_util.cpp

        src/internal/common/constants.cpp
        src/internal/common/document_cursor.cpp
        src/internal/common/document_path.cpp
        src/internal/common/file.cpp
        src/internal/common/filesystem.cpp
        src/internal/common/path.cpp
        src/internal/common/style.cpp
        src/internal/common/table_cursor.cpp
        src/internal/common/table_position.cpp
        src/internal/common/table_range.cpp

        src/internal/crypto/crypto_util.cpp

        src/internal/html/common.cpp
        src/internal/html/css_writer.cpp
        src/internal/html/document.cpp
        src/internal/html/document_style.cpp
        src/internal/html/document_element.cpp
        src/internal/html/html_writer.cpp

        src/internal/odf/odf_crypto.cpp
        src/internal/odf/odf_cursor.cpp
        src/internal/odf/odf_document.cpp
        src/internal/odf/odf_element.cpp
        src/internal/odf/odf_file.cpp
        src/internal/odf/odf_manifest.cpp
        src/internal/odf/odf_meta.cpp
        src/internal/odf/odf_style.cpp

        src/internal/oldms/oldms_file.cpp

        src/internal/ooxml/presentation/ooxml_presentation_document.cpp
        src/internal/ooxml/presentation/ooxml_presentation_element.cpp
        src/internal/ooxml/presentation/ooxml_presentation_cursor.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.cpp
        src/internal/ooxml/text/ooxml_text_cursor.cpp
        src/internal/ooxml/text/ooxml_text_document.cpp
        src/internal/ooxml/text/ooxml_text_element.cpp
        src/internal/ooxml/text/ooxml_text_style.cpp
        src/internal/ooxml/ooxml_crypto.cpp
        src/internal/ooxml/ooxml_file.cpp
        src/internal/ooxml/ooxml_meta.cpp
        src/internal/ooxml/ooxml_util.cpp

        src/internal/svm/svm_file.cpp
        src/internal/svm/svm_format.cpp
        src/internal/svm/svm_to_svg.cpp

        src/internal/util/file_util.cpp
        src/internal/util/hash_util.cpp
        src/internal/util/odr_meta_util.cpp
        src/internal/util/stream_util.cpp
        src/internal/util/string_util.cpp
        src/internal/util/xml_util.cpp

        src/internal/zip/zip_util.cpp
        src/internal/zip/zip_archive.cpp
        )
target_include_directories(odr-object
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr-object
        PRIVATE
        CONAN_PKG::pugixml
        CONAN_PKG::miniz
        CONAN_PKG::glog
        CONAN_PKG::cryptopp
        CONAN_PKG::nlohmann_json
        )
set_property(TARGET odr-object PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(odr-static STATIC
        $<TARGET_OBJECTS:odr-object>
        )
target_include_directories(odr-static
        PUBLIC
        include
        )
target_link_libraries(odr-static
        PRIVATE
        CONAN_PKG::pugixml
        CONAN_PKG::miniz
        CONAN_PKG::glog
        CONAN_PKG::cryptopp
        CONAN_PKG::nlohmann_json
        )

add_library(odr-shared SHARED
        $<TARGET_OBJECTS:odr-object>
        )
target_include_directories(odr-shared
        PUBLIC
        include
        )
target_link_libraries(odr-shared
        PRIVATE
        CONAN_PKG::pugixml
        CONAN_PKG::miniz
        CONAN_PKG::glog
        CONAN_PKG::cryptopp
        CONAN_PKG::nlohmann_json
        )

add_subdirectory(cli)

if (ODR_TEST)
    add_subdirectory(test)
endif ()

install(
        TARGETS
        odr-static
        odr-shared
        meta
        translate
        back_translate
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION bin
        LIBRARY DESTINATION lib
)
