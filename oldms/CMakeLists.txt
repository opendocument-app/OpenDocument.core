add_library(odr_oldms STATIC
        src/LegacyMicrosoft.cpp
        )
target_include_directories(odr_oldms
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr_oldms
        PRIVATE
        odr_access
        odr_common
        odr_crypto

        odr-interface
        )
set_property(TARGET odr_oldms PROPERTY POSITION_INDEPENDENT_CODE ON)
