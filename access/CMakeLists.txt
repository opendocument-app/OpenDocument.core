add_library(odr_access STATIC
        src/CfbStorage.cpp
        src/ChildStorage.cpp
        src/FileUtil.cpp
        src/Path.cpp
        src/StorageUtil.cpp
        src/StreamUtil.cpp
        src/SystemStorage.cpp
        src/ZipStorage.cpp
        )
target_include_directories(odr_access PUBLIC include)
target_link_libraries(odr_access
        PRIVATE
        miniz

        odr_crypto

        odr-interface
        )
set_property(TARGET odr_access PROPERTY POSITION_INDEPENDENT_CODE ON)
