FetchContent_Declare(
        csv-parser
        GIT_REPOSITORY https://github.com/andiwand/csv-parser.git
        GIT_TAG fix/cmake
)

FetchContent_GetProperties(csv-parser)
if (NOT csv-parser_POPULATED)
    FetchContent_Populate(csv-parser)
    set(CSV_DEVELOPER FALSE CACHE BOOL "" FORCE)
    add_subdirectory(${csv-parser_SOURCE_DIR} ${csv-parser_BINARY_DIR} EXCLUDE_FROM_ALL)
    target_include_directories(csv PUBLIC "${csv-parser_SOURCE_DIR}/include")
    set_property(TARGET csv PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()
