add_library(odr_odf STATIC
        src/OpenDocumentContentTranslator.cpp
        src/OpenDocumentCrypto.cpp
        src/OpenDocumentMeta.cpp
        src/OpenDocumentStyleTranslator.cpp
        src/OpenDocumentTranslator.cpp
        )
target_include_directories(odr_odf PUBLIC include)
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
