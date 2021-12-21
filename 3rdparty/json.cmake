FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.9.1
)

FetchContent_GetProperties(json)
if (NOT json_POPULATED)
    FetchContent_Populate(json)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
