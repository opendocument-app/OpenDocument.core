add_executable(meta src/meta.cpp)
target_link_libraries(meta
        PRIVATE
        nlohmann_json::nlohmann_json

        odr-static
        )

add_executable(translate src/translate.cpp)
target_link_libraries(translate
        PRIVATE
        odr-static
        )

add_executable(back_translate src/back_translate.cpp)
target_link_libraries(back_translate
        PRIVATE
        odr_access
        odr-static
        )
