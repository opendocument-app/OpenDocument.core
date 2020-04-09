add_library(odr-interface INTERFACE)
target_include_directories(odr-interface INTERFACE include)

add_library(odr STATIC
        src/OpenDocumentReader.cpp
        src/Meta.cpp
        )
target_include_directories(odr PUBLIC include)
target_link_libraries(odr
        PRIVATE
        tinyxml2
        miniz
        glog
        cryptopp-static
        nlohmann_json::nlohmann_json

        odr_access
        odr_common
        odr_crypto
        odr_odf
        odr_ooxml
        )
set_property(TARGET odr PROPERTY POSITION_INDEPENDENT_CODE ON)
