add_library(odr_generic STATIC
        src/GenericDocument.cpp
        )
target_include_directories(odr_generic PUBLIC include)
target_link_libraries(odr_generic
        PRIVATE
        )
set_property(TARGET odr_generic PROPERTY POSITION_INDEPENDENT_CODE ON)
