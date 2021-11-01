include(FetchContent)

FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.11.1
)
FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        #GIT_TAG 2.1.0
        # TODO switch to upcoming release
        GIT_TAG 4159f8c
)
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
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.9.1
)
FetchContent_Declare(
        uchardet
        GIT_REPOSITORY https://github.com/freedesktop/uchardet.git
        GIT_TAG v0.0.7
)
FetchContent_Declare(
        csv-parser
        GIT_REPOSITORY https://github.com/andiwand/csv-parser.git
        GIT_TAG fix/cmake
)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.10.0
)

# cryptopp: fixes "phony targets" duplicate error during Android build
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# miniz
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_GetProperties(pugixml)
if (NOT pugixml_POPULATED)
    FetchContent_Populate(pugixml)
    add_subdirectory(${pugixml_SOURCE_DIR} ${pugixml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
FetchContent_GetProperties(miniz)
if (NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${miniz_SOURCE_DIR} ${miniz_BINARY_DIR} EXCLUDE_FROM_ALL)
    # miniz defines a lot of macros without that
    target_compile_definitions(miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
    set_property(TARGET miniz PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()
FetchContent_GetProperties(cryptopp)
FetchContent_GetProperties(cryptopp_cmake)
if (NOT cryptopp_POPULATED)
    FetchContent_Populate(cryptopp)
    FetchContent_Populate(cryptopp_cmake)

    file(COPY ${cryptopp_cmake_SOURCE_DIR}/CMakeLists.txt DESTINATION ${cryptopp_SOURCE_DIR})
    file(COPY ${cryptopp_cmake_SOURCE_DIR}/cryptopp-config.cmake DESTINATION ${cryptopp_SOURCE_DIR})

    add_subdirectory(${cryptopp_SOURCE_DIR} ${cryptopp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
FetchContent_GetProperties(json)
if (NOT json_POPULATED)
    FetchContent_Populate(json)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
FetchContent_GetProperties(uchardet)
if (NOT uchardet_POPULATED)
    FetchContent_Populate(uchardet)
    add_subdirectory(${uchardet_SOURCE_DIR} ${uchardet_BINARY_DIR} EXCLUDE_FROM_ALL)
    target_include_directories(libuchardet PUBLIC "${uchardet_SOURCE_DIR}/src")
    set_property(TARGET libuchardet PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()

if (ODR_TEST)
    FetchContent_GetProperties(csv-parser)
    if (NOT csv-parser_POPULATED)
        FetchContent_Populate(csv-parser)
        set(CSV_DEVELOPER FALSE CACHE BOOL "" FORCE)
        add_subdirectory(${csv-parser_SOURCE_DIR} ${csv-parser_BINARY_DIR} EXCLUDE_FROM_ALL)
        target_include_directories(csv PUBLIC "${csv-parser_SOURCE_DIR}/include")
    endif ()
    FetchContent_GetProperties(googletest)
    if (NOT googletest_POPULATED)
        FetchContent_Populate(googletest)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(gtest_build_tests OFF CACHE BOOL "" FORCE)
        set(gtest_build_samples OFF CACHE BOOL "" FORCE)
        add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif ()
endif ()
