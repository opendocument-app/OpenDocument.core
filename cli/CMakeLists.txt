add_executable(translate src/translate.cpp)
target_link_libraries(translate
        PUBLIC
        odr
        )

add_executable(back_translate src/back_translate.cpp)
target_link_libraries(back_translate
        PUBLIC
        odr_access
        odr
        )
