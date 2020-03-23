add_library(odr_ooxml STATIC
        src/OfficeOpenXmlCrypto.cpp
        src/OfficeOpenXmlDocumentTranslator.cpp
        src/OfficeOpenXmlMeta.cpp
        src/OfficeOpenXmlPresentationTranslator.cpp
        src/OfficeOpenXmlTranslator.cpp
        src/OfficeOpenXmlWorkbookTranslator.cpp
)
target_include_directories(odr_ooxml PUBLIC include)
target_link_libraries(odr_ooxml
    PRIVATE
        tinyxml2
        glog

    PUBLIC
        odr_access
        odr_common
        odr_crypto

        odr-interface
)
