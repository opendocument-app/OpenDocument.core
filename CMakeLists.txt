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
        src/Document.cpp
        src/DocumentElements.cpp
        src/File.cpp
        src/FileNoExcept.cpp
        src/Html.cpp
        src/OpenDocument.cpp
        src/OpenStrategy.cpp

        src/access/CfbStorage.cpp
        src/access/ChildStorage.cpp
        src/access/FileUtil.cpp
        src/access/Path.cpp
        src/access/StorageUtil.cpp
        src/access/StreamUtil.cpp
        src/access/SystemStorage.cpp
        src/access/ZipStorage.cpp

        src/common/Constants.cpp
        src/common/DocumentElements.cpp
        src/common/File.cpp
        src/common/Html.cpp
        src/common/StringUtil.cpp
        src/common/TableCursor.cpp
        src/common/TablePosition.cpp
        src/common/TableRange.cpp
        src/common/XmlUtil.cpp

        src/crypto/CryptoUtil.cpp

        src/odf/Common.cpp
        src/odf/ContentTranslator.cpp
        src/odf/Crypto.cpp
        src/odf/Manifest.cpp
        src/odf/Meta.cpp
        src/odf/OpenDocument.cpp
        src/odf/OpenDocumentFile.cpp
        src/odf/Style.cpp
        src/odf/StyleTranslator.cpp

        src/oldms/LegacyMicrosoftFile.cpp

        src/ooxml/Crypto.cpp
        src/ooxml/DocumentTranslator.cpp
        src/ooxml/Meta.cpp
        src/ooxml/OfficeOpenXmlFile.cpp
        src/ooxml/PresentationTranslator.cpp
        src/ooxml/WorkbookTranslator.cpp

        src/svm/Svm2Svg.cpp
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
