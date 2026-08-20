# WebAssembly bindings for OpenDocument.core (npm package `@opendocument/odr-core`).
#
# Included from the top-level CMakeLists.txt when `ODR_WASM` is ON. Can also be
# configured standalone against an installed `odrcore`.

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    project(odr_wasm LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(odrcore REQUIRED)
    set(ODR_WASM_ODR_TARGET odrcore::odrcore)
else ()
    set(ODR_WASM_ODR_TARGET odr)
endif ()

if (NOT EMSCRIPTEN)
    message(FATAL_ERROR
            "ODR_WASM needs the Emscripten toolchain. Configure with the "
            "`emscripten-wasm` conan profile "
            "(.github/config/conan/profiles/emscripten-wasm).")
endif ()

# The `.wasm` is the only artifact; there is no such thing as linking odrcore
# beside it. Same reasoning as `apple/CMakeLists.txt`.
if (BUILD_SHARED_LIBS)
    message(FATAL_ERROR "ODR_WASM needs BUILD_SHARED_LIBS=OFF")
endif ()

add_executable(odr_wasm
        "src/odr_wasm.cpp"
        "src/wasm_core.cpp"
        "src/wasm_file.cpp"
        "src/wasm_html.cpp"
        "src/wasm_logger.cpp"
)
# `odr_meta_util.hpp` is reused for the meta blob and returns a json object, so
# the binding needs the header the library keeps private.
find_package(nlohmann_json REQUIRED)

target_link_libraries(odr_wasm PRIVATE
        ${ODR_WASM_ODR_TARGET}
        nlohmann_json::nlohmann_json
        embind
)
target_include_directories(odr_wasm PRIVATE "src")

# `-sEXPORT_ES6` requires the `.mjs` suffix. The `.wasm` stays a separate file
# rather than `-sSINGLE_FILE`: base64 inside the glue costs a third more bytes
# and gives up both streaming compilation and edge caching, which matter more
# than the extra request.
set_target_properties(odr_wasm PROPERTIES
        OUTPUT_NAME "odr-core"
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/dist>"
)
target_link_options(odr_wasm PRIVATE
        --bind
        --no-entry
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createOdrModule
        -sENVIRONMENT=web,worker,node
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=33554432
        -sMAXIMUM_MEMORY=4294967296
        # Emscripten defaults to 64 KB. The element-registry builders, the
        # renderer's tree walk and the PDF object parser all recurse, and the
        # failure at 64 KB does not look like a stack overflow.
        -sSTACK_SIZE=8388608
        -sFILESYSTEM=1
        # embind builds its invokers with `new Function`, which a
        # Content-Security-Policy treats as eval - so loading the module needed
        # `script-src 'unsafe-eval'`, the one thing a page embedding a local
        # document renderer would rather not grant. This picks embind's
        # closure-based path instead, leaving `'wasm-unsafe-eval'` enough.
        # Nothing here calls `emscripten_run_script`, `dlopen`, `ccall` or
        # `cwrap`, which is what it costs.
        -sDYNAMIC_EXECUTION=0
)

# The hand-written half of the package sits beside the generated glue, so the
# build directory is directly importable and `npm pack` has one source.
add_custom_target(odr_wasm_package
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/js" "${CMAKE_CURRENT_BINARY_DIR}/dist"
)
add_dependencies(odr_wasm odr_wasm_package)

if (ODR_TEST)
    enable_testing()

    # Look outside the conan search path first. `emsdk` pulls in `nodejs/16.3.0`
    # as a tool requirement, and CMakeToolchain puts its `bin` on
    # `CMAKE_PROGRAM_PATH` — so a plain `find_program` finds a node that predates
    # `--test` and the suite fails as "bad option" rather than as a test.
    find_program(ODR_WASM_NODE NAMES node nodejs
            NO_CMAKE_PATH NO_CMAKE_SYSTEM_PATH)
    if (NOT ODR_WASM_NODE)
        find_program(ODR_WASM_NODE NAMES node nodejs)
    endif ()

    set(ODR_WASM_NODE_MINIMUM 18) # `node --test`

    if (ODR_WASM_NODE)
        execute_process(COMMAND "${ODR_WASM_NODE}" --version
                OUTPUT_VARIABLE ODR_WASM_NODE_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
        string(REGEX REPLACE "^v" "" ODR_WASM_NODE_VERSION "${ODR_WASM_NODE_VERSION}")
    endif ()

    if (ODR_WASM_NODE AND ODR_WASM_NODE_VERSION
            AND NOT ODR_WASM_NODE_VERSION VERSION_LESS ODR_WASM_NODE_MINIMUM)
        # No path argument: node discovers `*.test.mjs` under the working
        # directory itself. Passing the directory instead works on node 20 but
        # not on 22, which reads a bare path as a module to execute and fails
        # with `Cannot find module`.
        add_test(NAME odr_wasm_node COMMAND "${ODR_WASM_NODE}" --test)
        set_tests_properties(odr_wasm_node PROPERTIES
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests"
                ENVIRONMENT "ODR_WASM_DIST=${CMAKE_CURRENT_BINARY_DIR}/dist")
    else ()
        # Left as a warning rather than an error so a contributor without node
        # can still build. CI passes `--no-tests=error` to ctest, so skipping
        # there is loud.
        message(WARNING
                "no node >= ${ODR_WASM_NODE_MINIMUM} "
                "(found '${ODR_WASM_NODE}' ${ODR_WASM_NODE_VERSION}); "
                "skipping the odr_wasm test registration")
    endif ()
endif ()

install(TARGETS odr_wasm RUNTIME DESTINATION dist COMPONENT wasm)
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/dist/odr-core.wasm"
        DESTINATION dist COMPONENT wasm)
