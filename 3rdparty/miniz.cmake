FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        #GIT_TAG 2.1.0
        # TODO switch to upcoming release
        GIT_TAG 4159f8c
)

FetchContent_GetProperties(miniz)
if (NOT miniz_POPULATED)
    FetchContent_Populate(miniz)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${miniz_SOURCE_DIR} ${miniz_BINARY_DIR} EXCLUDE_FROM_ALL)
    # miniz defines a lot of macros without that
    target_compile_definitions(miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
    set_property(TARGET miniz PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()
