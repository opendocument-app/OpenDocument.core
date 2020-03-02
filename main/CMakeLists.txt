add_executable(translate translate.cpp)
target_include_directories(translate PRIVATE ../src)
target_link_libraries(translate PRIVATE odrlib)

add_executable(back_translate back_translate.cpp)
target_include_directories(back_translate PRIVATE ../src)
target_link_libraries(back_translate PRIVATE odrlib)
