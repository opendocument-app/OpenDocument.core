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
        src/document_elements.cpp
        src/document_style.cpp
        src/file.cpp
        src/file_no_except.cpp
        src/html.cpp
        src/open_document_reader.cpp
        src/open_strategy.cpp
        src/property.cpp

        src/abstract/document_elements.cpp
        src/abstract/file.cpp
        src/abstract/property.cpp

        src/cfb/cfb_archive.cpp
        src/cfb/cfb_impl.cpp
        src/cfb/cfb_util.cpp

        src/common/constants.cpp
        src/common/file.cpp
        src/common/filesystem.cpp
        src/common/html.cpp
        src/common/path.cpp
        src/common/property.cpp
        src/common/table_cursor.cpp
        src/common/table_position.cpp
        src/common/table_range.cpp

        src/crypto/crypto_util.cpp

        src/odf/odf_crypto.cpp
        src/odf/odf_elements.cpp
        src/odf/odf_manifest.cpp
        src/odf/odf_meta.cpp
        src/odf/odf_document.cpp
        src/odf/odf_document_file.cpp
        src/odf/odf_style.cpp

        src/oldms/oldms_document_file.cpp

        src/ooxml/ooxml_crypto.cpp
        src/ooxml/ooxml_document.cpp
        src/ooxml/ooxml_document_file.cpp
        src/ooxml/ooxml_meta.cpp
        src/ooxml/text/ooxml_text_elements.cpp
        src/ooxml/text/ooxml_text_style.cpp

        src/svm/svm_file.cpp
        src/svm/svm_format.cpp
        src/svm/svm_to_svg.cpp

        src/util/file_util.cpp
        src/util/stream_util.cpp
        src/util/string_util.cpp
        src/util/xml_util.cpp

        src/zip/miniz_util.cpp
        src/zip/zip_archive.cpp
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
