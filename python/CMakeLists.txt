# Python bindings for OpenDocument.core (package `pyodr`).
#
# Included from the top-level CMakeLists.txt when `ODR_PYTHON` is ON. Can also
# be configured standalone against an installed `odrcore` package.

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    project(pyodr LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(odrcore REQUIRED)
    set(ODR_PYTHON_ODR_TARGET odrcore::odrcore)

    # Same option name as the root build; an installed odrcore ships
    # `http_server.hpp` only when it was built with the HTTP server.
    option(ODR_WITH_HTTP_SERVER "Build with cpp-httplib HTTP server" ON)
else ()
    set(ODR_PYTHON_ODR_TARGET odr)
endif ()

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

if (ODR_WITH_HTTP_SERVER)
    set(ODR_PYTHON_HTTP_SERVER_SOURCE "src/bind_http_server.cpp")
else ()
    set(ODR_PYTHON_HTTP_SERVER_SOURCE "src/bind_http_server_stub.cpp")
endif ()

pybind11_add_module(pyodr_core
        "src/module.cpp"
        "src/bind_core.cpp"
        "src/bind_document.cpp"
        "src/bind_file.cpp"
        "src/bind_html.cpp"
        "src/bind_logger.cpp"
        "${ODR_PYTHON_HTTP_SERVER_SOURCE}"
        "src/bind_style.cpp"
)
target_link_libraries(pyodr_core PRIVATE ${ODR_PYTHON_ODR_TARGET})
# The `$<1:...>` genex keeps multi-config generators from appending a
# per-config subdirectory; the module must sit inside the `pyodr` package.
set_target_properties(pyodr_core PROPERTIES
        OUTPUT_NAME "_core"
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/pyodr>"
)

# Mirror the pure-python package next to the built extension so the build tree
# is directly importable: PYTHONPATH=<build>/python
add_custom_target(pyodr_package
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/pyodr" "${CMAKE_CURRENT_BINARY_DIR}/pyodr"
)
add_dependencies(pyodr_core pyodr_package)

install(TARGETS pyodr_core LIBRARY DESTINATION pyodr COMPONENT python)
install(DIRECTORY pyodr/ DESTINATION pyodr COMPONENT python
        FILES_MATCHING PATTERN "*.py" PATTERN "py.typed")
if (ODR_TEST)
    enable_testing()
    add_test(NAME pyodr_pytest
            COMMAND Python::Interpreter -m pytest "${CMAKE_CURRENT_SOURCE_DIR}/tests" -v)
    set_tests_properties(pyodr_pytest PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}")
endif ()
