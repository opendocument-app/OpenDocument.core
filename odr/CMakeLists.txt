# TODO use object library
add_library(odr-static STATIC
        src/OpenDocumentReader.cpp
        )
target_include_directories(odr-static PUBLIC include)
target_link_libraries(odr-static PRIVATE
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
set_property(TARGET odr-static PROPERTY POSITION_INDEPENDENT_CODE ON)
