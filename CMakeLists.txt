cmake_minimum_required(VERSION 3.12.4)
project(odr
        VERSION 2.0.0
        LANGUAGES C CXX
        )
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ODR_TEST "enable tests" ON)

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
endif ()

# use ccache if available
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_FOUND}")
    message(STATUS "using ccache ${CCACHE_FOUND}")
endif (CCACHE_FOUND)

if (NOT DEFINED CONAN_EXPORTED)
    include(cmake/conan.cmake)
    conan_cmake_configure(GENERATORS cmake)
    conan_cmake_autodetect(settings)
    conan_cmake_install(PATH_OR_REFERENCE ..
            BUILD missing
            SETTINGS ${settings}
            SETTINGS compiler.cppstd=17
            ENV CC=${CMAKE_C_COMPILER}
            ENV CXX=${CMAKE_CXX_COMPILER})
    #include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    #conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
    include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif ()

find_package(pugixml REQUIRED)
find_package(miniz REQUIRED)
find_package(cryptopp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(vincentlaucsb-csv-parser REQUIRED)
find_package(uchardet REQUIRED)

configure_file(src/internal/project_info.cpp.in src/internal/project_info.cpp)

set(PRE_CONFIGURE_FILE "src/internal/git_info.cpp.in")
set(POST_CONFIGURE_FILE "${CMAKE_CURRENT_BINARY_DIR}/src/internal/git_info.cpp")
if (EXISTS ${PROJECT_SOURCE_DIR}/.git)
    include(cmake/git_watcher.cmake)
else ()
    if (NOT DEFINED GIT_HEAD_SHA1)
        set(GIT_HEAD_SHA1 "unknown")
    endif ()
    if (NOT DEFINED GIT_IS_DIRTY)
        set(GIT_IS_DIRTY "false")
    endif ()
    configure_file("${PRE_CONFIGURE_FILE}" "${POST_CONFIGURE_FILE}" @ONLY)
endif ()

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
        src/quantity.cpp
        src/style.cpp

        ${CMAKE_CURRENT_BINARY_DIR}/src/internal/git_info.cpp
        src/internal/magic.cpp
        src/internal/open_strategy.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/src/internal/project_info.cpp
        src/internal/resource.cpp
        src/internal/resource_data.cpp

        src/internal/cfb/cfb_archive.cpp
        src/internal/cfb/cfb_impl.cpp
        src/internal/cfb/cfb_util.cpp

        src/internal/common/document_cursor.cpp
        src/internal/common/document_path.cpp
        src/internal/common/file.cpp
        src/internal/common/filesystem.cpp
        src/internal/common/image_file.cpp
        src/internal/common/path.cpp
        src/internal/common/random.cpp
        src/internal/common/style.cpp
        src/internal/common/table_cursor.cpp
        src/internal/common/table_position.cpp
        src/internal/common/table_range.cpp
        src/internal/common/temporary_file.cpp

        src/internal/crypto/crypto_util.cpp

        src/internal/csv/csv_file.cpp
        src/internal/csv/csv_util.cpp

        src/internal/html/common.cpp
        src/internal/html/css_writer.cpp
        src/internal/html/document.cpp
        src/internal/html/document_style.cpp
        src/internal/html/document_element.cpp
        src/internal/html/image_file.cpp
        src/internal/html/text_file.cpp

        src/internal/json/json_file.cpp
        src/internal/json/json_util.cpp

        src/internal/odf/odf_crypto.cpp
        src/internal/odf/odf_cursor.cpp
        src/internal/odf/odf_document.cpp
        src/internal/odf/odf_element.cpp
        src/internal/odf/odf_file.cpp
        src/internal/odf/odf_manifest.cpp
        src/internal/odf/odf_meta.cpp
        src/internal/odf/odf_style.cpp

        src/internal/oldms/oldms_file.cpp

        src/internal/ooxml/presentation/ooxml_presentation_cursor.cpp
        src/internal/ooxml/presentation/ooxml_presentation_document.cpp
        src/internal/ooxml/presentation/ooxml_presentation_element.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_cursor.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.cpp
        src/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.cpp
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

        src/internal/text/text_file.cpp
        src/internal/text/text_util.cpp

        src/internal/util/file_util.cpp
        src/internal/util/hash_util.cpp
        src/internal/util/odr_meta_util.cpp
        src/internal/util/stream_util.cpp
        src/internal/util/string_util.cpp
        src/internal/util/xml_util.cpp

        src/internal/zip/zip_util.cpp
        src/internal/zip/zip_archive.cpp
        )
set_target_properties(odr-object PROPERTIES OUTPUT_NAME odr)
if (EXISTS ${PROJECT_SOURCE_DIR}/.git)
    add_dependencies(odr-object check_git)
endif ()
target_include_directories(odr-object
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr-object
        PRIVATE
        pugixml::pugixml
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        vincentlaucsb-csv-parser::vincentlaucsb-csv-parser
        uchardet::uchardet
        )
set_property(TARGET odr-object PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(odr-static STATIC
        $<TARGET_OBJECTS:odr-object>
        )
set_target_properties(odr-static PROPERTIES OUTPUT_NAME odr)
target_include_directories(odr-static
        PUBLIC
        include
        )
target_link_libraries(odr-static
        PRIVATE
        pugixml::pugixml
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        vincentlaucsb-csv-parser::vincentlaucsb-csv-parser
        uchardet::uchardet
        )

add_library(odr-shared SHARED
        $<TARGET_OBJECTS:odr-object>
        )
set_target_properties(odr-shared PROPERTIES OUTPUT_NAME odr)
target_include_directories(odr-shared
        PUBLIC
        include
        )
target_link_libraries(odr-shared
        PRIVATE
        pugixml::pugixml
        miniz::miniz
        cryptopp::cryptopp
        nlohmann_json::nlohmann_json
        vincentlaucsb-csv-parser::vincentlaucsb-csv-parser
        uchardet::uchardet
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
