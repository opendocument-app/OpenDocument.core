FetchContent_Declare(
        cryptopp_cmake
        GIT_REPOSITORY https://github.com/noloader/cryptopp-cmake.git
        #GIT_TAG CRYPTOPP_8_2_0
        # TODO switch to upcoming release
        GIT_TAG 85941c6
)
FetchContent_Declare(
        cryptopp
        GIT_REPOSITORY https://github.com/weidai11/cryptopp.git
        #GIT_TAG CRYPTOPP_8_2_0
        # TODO switch to upcoming release
        GIT_TAG 5d68850
)

FetchContent_GetProperties(cryptopp)
FetchContent_GetProperties(cryptopp_cmake)
if (NOT cryptopp_POPULATED)
    FetchContent_Populate(cryptopp)
    FetchContent_Populate(cryptopp_cmake)

    file(COPY ${cryptopp_cmake_SOURCE_DIR}/CMakeLists.txt DESTINATION ${cryptopp_SOURCE_DIR})
    file(COPY ${cryptopp_cmake_SOURCE_DIR}/cryptopp-config.cmake DESTINATION ${cryptopp_SOURCE_DIR})

    add_subdirectory(${cryptopp_SOURCE_DIR} ${cryptopp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
