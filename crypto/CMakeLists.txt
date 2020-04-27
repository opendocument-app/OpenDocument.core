add_library(odr_crypto STATIC
        src/CryptoUtil.cpp
        )
target_include_directories(odr_crypto PUBLIC include)
target_link_libraries(odr_crypto
        PRIVATE
        cryptopp-static
        )
set_property(TARGET odr_crypto PROPERTY POSITION_INDEPENDENT_CODE ON)
