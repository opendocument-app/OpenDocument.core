cmake_minimum_required(VERSION 3.12.4)
project(odr C CXX)
set(CMAKE_CXX_STANDARD 17)

option(ODR_TEST "enable tests" ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -D_GLIBCXX_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# use ccache if available
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif (CCACHE_FOUND)

add_subdirectory(3rdparty)

add_library(odr-interface INTERFACE)
target_include_directories(odr-interface INTERFACE include)

add_library(odr-object OBJECT
        src/document.cpp
        src/exceptions.cpp
        src/file.cpp
        src/file_no_except.cpp
        src/html.cpp
        src/open_document_reader.cpp
        src/open_strategy.cpp

        src/internal/abstract/file.cpp

        src/internal/cfb/cfb_archive.cpp
        src/internal/cfb/cfb_impl.cpp
        src/internal/cfb/cfb_util.cpp

        src/internal/common/constants.cpp
        src/internal/common/file.cpp
        src/internal/common/filesystem.cpp
        src/internal/common/html.cpp
        src/internal/common/path.cpp
        src/internal/common/table_cursor.cpp
        src/internal/common/table_position.cpp
        src/internal/common/table_range.cpp

        src/internal/crypto/crypto_util.cpp

        src/internal/odf/odf_crypto.cpp
        src/internal/odf/odf_document.cpp
        src/internal/odf/odf_file.cpp
        src/internal/odf/odf_manifest.cpp
        src/internal/odf/odf_meta.cpp
        src/internal/odf/odf_style.cpp
        src/internal/odf/odf_table.cpp

        src/internal/oldms/oldms_file.cpp

        src/internal/ooxml/ooxml_crypto.cpp
        src/internal/ooxml/ooxml_file.cpp
        src/internal/ooxml/ooxml_meta.cpp
        src/internal/ooxml/ooxml_text_document.cpp

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
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json
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
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json
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
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json
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
