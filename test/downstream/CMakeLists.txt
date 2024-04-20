cmake_minimum_required(VERSION 3.12.4)

project(odr-test-downstream LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED CONAN_EXPORTED)
    include("cmake/conan.cmake")
    conan_cmake_configure(GENERATORS cmake)
    conan_cmake_autodetect(settings)
    conan_cmake_install(PATH_OR_REFERENCE ".."
            BUILD missing
            SETTINGS ${settings}
            ENV CC=${CMAKE_C_COMPILER}
            ENV CXX=${CMAKE_CXX_COMPILER})
    include("${CMAKE_BINARY_DIR}/conan_paths.cmake")
endif ()

find_package(odr REQUIRED)

add_executable(odr-test-downstream main.cpp)
target_link_libraries(odr-test-downstream PRIVATE odr::odr)
