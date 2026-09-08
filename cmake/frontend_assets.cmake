# Generates the header that holds the renderer's stylesheets and scripts as
# `std::string_view`s, one per file in `src/odr/internal/html/frontend/`.
#
# Run in script mode:
#   cmake -DASSET_DIR=<dir> -DASSETS=<name|name|…> -DOUTPUT=<header> \
#         -P cmake/frontend_assets.cmake
#
# The bytes become a `char` array rather than a string literal: msvc caps a
# literal at 16380 bytes and an array at nothing.

cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED ASSET_DIR OR NOT DEFINED ASSETS OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "ASSET_DIR, ASSETS and OUTPUT are all required")
endif ()

string(REPLACE "|" ";" ASSETS "${ASSETS}")

# Sixteen `'\x..',` in a row: cmake's regex has no `{n}` repetition, and `.`
# stands in for the backslash, which two levels of escaping would eat.
set(_row_pattern "")
foreach (_ RANGE 15)
    string(APPEND _row_pattern "'....',")
endforeach ()

set(_storage "")
set(_views "")

foreach (_asset IN LISTS ASSETS)
    set(_path "${ASSET_DIR}/${_asset}")
    if (NOT EXISTS "${_path}")
        message(FATAL_ERROR "no such frontend asset: ${_path}")
    endif ()

    file(READ "${_path}" _hex HEX)
    if (_hex STREQUAL "")
        message(FATAL_ERROR "empty frontend asset: ${_path}")
    endif ()

    string(REGEX REPLACE "\\.|-" "_" _name "${_asset}")

    # A character literal, not an integer: a byte over 0x7f narrows on a
    # platform whose `char` is signed, and a negative one where it is unsigned.
    string(REGEX REPLACE "(..)" "'\\\\x\\1'," _bytes "${_hex}")
    string(REGEX REPLACE "(${_row_pattern})" "\\1\n    " _bytes "${_bytes}")

    string(APPEND _storage
            "inline constexpr char ${_name}[]{\n    ${_bytes}\n};\n\n")
    string(APPEND _views
            "inline constexpr std::string_view ${_name}{\n"
            "    storage::${_name}, sizeof(storage::${_name})};\n")
endforeach ()

set(_content "// Generated from src/odr/internal/html/frontend/ by\n\
// cmake/frontend_assets.cmake. Do not edit.\n\
\n\
#pragma once\n\
\n\
#include <string_view>\n\
\n\
namespace odr::internal::html::frontend_assets {\n\
\n\
/// The bytes themselves, which nothing outside this header names.\n\
namespace storage {\n\
\n\
${_storage}} // namespace storage\n\
\n\
${_views}\n\
} // namespace odr::internal::html::frontend_assets\n")

file(WRITE "${OUTPUT}" "${_content}")
