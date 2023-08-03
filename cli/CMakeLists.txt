add_executable(meta src/meta.cpp)
target_link_libraries(meta
        PRIVATE
        odr
        nlohmann_json::nlohmann_json
        )
target_include_directories(meta
        PRIVATE
        ../src
        )

add_executable(translate src/translate.cpp)
target_link_libraries(translate
        PRIVATE
        odr
        )

add_executable(back_translate src/back_translate.cpp)
target_link_libraries(back_translate
        PRIVATE
        odr
        )
target_include_directories(back_translate
        PRIVATE
        ../src
        )
