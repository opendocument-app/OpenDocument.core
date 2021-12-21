FetchContent_Declare(
        uchardet
        GIT_REPOSITORY https://github.com/freedesktop/uchardet.git
        GIT_TAG d7dad54
)

FetchContent_GetProperties(uchardet)
if (NOT uchardet_POPULATED)
    FetchContent_Populate(uchardet)
    add_subdirectory(${uchardet_SOURCE_DIR} ${uchardet_BINARY_DIR} EXCLUDE_FROM_ALL)
    set_property(TARGET libuchardet PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()
