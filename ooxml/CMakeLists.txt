add_library(odr_ooxml STATIC
        src/Crypto.cpp
        src/DocumentTranslator.cpp
        src/Meta.cpp
        src/OfficeOpenXml.cpp
        src/PresentationTranslator.cpp
        src/WorkbookTranslator.cpp
        )
target_include_directories(odr_ooxml
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr_ooxml
        PRIVATE
        tinyxml2
        glog

        odr_access
        odr_common
        odr_crypto

        odr-interface
        )
set_property(TARGET odr_ooxml PROPERTY POSITION_INDEPENDENT_CODE ON)
