add_library(odr_html STATIC
        src/Html.cpp
        src/HtmlTranslation.cpp
        )
target_include_directories(odr_html PUBLIC include)
target_link_libraries(odr_html
        PUBLIC
        pugixml

        odr_common

        odr-interface
        )
set_property(TARGET odr_html PROPERTY POSITION_INDEPENDENT_CODE ON)
