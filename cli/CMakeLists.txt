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

add_executable(server src/server.cpp)
target_link_libraries(server
        PRIVATE
        httplib

        odr_access
        odr-static
        )
