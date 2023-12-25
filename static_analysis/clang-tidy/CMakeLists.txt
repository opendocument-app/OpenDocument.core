find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)

if (NOT CLANG_TIDY_COMMAND)
    message(ERROR "ACTS_RUN_CLANG_TIDY is ON but clang-tidy is not found!")
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
endif ()

message(STATUS "Setting up clang-tidy run")

set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
