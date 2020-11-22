add_library(odr-interface INTERFACE)
target_include_directories(odr-interface INTERFACE include)

add_library(odr-static STATIC
        src/Document.cpp
        src/DocumentElements.cpp
        src/DocumentNoExcept.cpp
        src/File.cpp
        src/FileNoExcept.cpp
        src/OpenDocument.cpp
        src/OpenStrategy.cpp
        )
target_include_directories(odr-static
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr-static
        PRIVATE
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json

        odr_access
        odr_common
        odr_crypto
        odr_odf
        odr_oldms
        odr_ooxml
        )

add_library(odr-shared SHARED
        src/Document.cpp
        src/DocumentNoExcept.cpp
        src/DocumentElements.cpp
        src/File.cpp
        src/FileNoExcept.cpp
        src/OpenDocument.cpp
        src/OpenStrategy.cpp
        )
target_link_libraries(odr-shared
        PRIVATE
        pugixml
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json

        odr_access
        odr_common
        odr_crypto
        odr_odf
        odr_oldms
        odr_ooxml
        )
target_include_directories(odr-shared
        PUBLIC
        include
        PRIVATE
        src
        )
