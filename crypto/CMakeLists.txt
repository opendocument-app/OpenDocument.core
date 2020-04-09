add_library(odr_crypto STATIC
        src/CfbCrypto.cpp
        src/CryptoUtil.cpp
        )
target_include_directories(odr_crypto PUBLIC include)
target_link_libraries(odr_crypto
        PRIVATE
        cryptopp-static
        )
