add_library(odr-interface INTERFACE)
target_include_directories(odr-interface INTERFACE include)

add_library(odr-static STATIC
        src/Document.cpp
        src/Meta.cpp
        src/Server.cpp
        )
target_include_directories(odr-static PUBLIC include)
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
        src/Meta.cpp
        src/Server.cpp
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
target_include_directories(odr-shared PUBLIC include)
