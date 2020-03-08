add_executable(translate src/translate.cpp)
target_link_libraries(translate PRIVATE
        odr-static
        )

add_executable(back_translate src/back_translate.cpp)
target_link_libraries(back_translate PRIVATE
        odr_access

        odr-static
        )
