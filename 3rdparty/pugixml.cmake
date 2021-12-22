FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.11.1
)

FetchContent_GetProperties(pugixml)
if (NOT pugixml_POPULATED)
    FetchContent_Populate(pugixml)
    add_subdirectory(${pugixml_SOURCE_DIR} ${pugixml_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
