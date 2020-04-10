add_library(odr_odf STATIC
        src/OpenDocument.cpp
        src/ContentTranslator.cpp
        src/Crypto.cpp
        src/Meta.cpp
        src/StyleTranslator.cpp
        )
target_include_directories(odr_odf
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr_odf
        PRIVATE
        tinyxml2
        nlohmann_json::nlohmann_json
        glog

        odr_access
        odr_common
        odr_crypto
        odr_svm

        odr-interface
        )
set_property(TARGET odr_odf PROPERTY POSITION_INDEPENDENT_CODE ON)
