#!/usr/bin/env python3
"""Generate the simple-font encoding tables and Adobe Glyph List for the PDF
module (stage 1.2 — simple-font encoding -> Unicode).

The output is committed C++ source (``pdf_encoding_data.{hpp,cpp}``); the build
only compiles it, so there is no build-time dependency on this script. Re-run it
whenever the source data changes:

    python3 tools/pdf/generate_encoding_data.py \
        --agl path/to/glyphlist.txt \
        --out src/odr/internal/pdf

``glyphlist.txt`` is Adobe's Adobe Glyph List
(https://github.com/adobe-type-tools/agl-aglfn). When ``--agl`` is omitted the
script emits only the SEED data embedded below (basic Latin + a few common
extras) so the module compiles and the unit tests pass; the full ~4,300-entry
AGL and the complete base-encoding tables (ISO 32000-1 Annex D) are populated by
re-running with the real source files. This staged seed is intentional — see
``pdf/AGENTS.md`` (stage 1.2).

NOTE (stage 1.2 scaffolding): the base-encoding tables below are seeded with the
ASCII range plus the handful of WinAnsi/Standard divergences that the tests
exercise. Completing them from ISO 32000-1 Annex D (the full Latin character
set, MacRoman, the Latin-1 upper half of WinAnsi) is a follow-up commit on this
branch.
"""

from __future__ import annotations

import argparse
import datetime
import os

# --- Seed glyph-name tables -------------------------------------------------

# ASCII punctuation / symbol glyph names shared by the base encodings (the
# letters and digits are generated programmatically below).
_ASCII_PUNCT = {
    0x20: "space",
    0x21: "exclam",
    0x22: "quotedbl",
    0x23: "numbersign",
    0x24: "dollar",
    0x25: "percent",
    0x26: "ampersand",
    0x28: "parenleft",
    0x29: "parenright",
    0x2A: "asterisk",
    0x2B: "plus",
    0x2C: "comma",
    0x2D: "hyphen",
    0x2E: "period",
    0x2F: "slash",
    0x3A: "colon",
    0x3B: "semicolon",
    0x3C: "less",
    0x3D: "equal",
    0x3E: "greater",
    0x3F: "question",
    0x40: "at",
    0x5B: "bracketleft",
    0x5C: "backslash",
    0x5D: "bracketright",
    0x5E: "asciicircum",
    0x5F: "underscore",
    0x7B: "braceleft",
    0x7C: "bar",
    0x7D: "braceright",
    0x7E: "asciitilde",
}

_DIGIT_NAMES = ["zero", "one", "two", "three", "four",
                "five", "six", "seven", "eight", "nine"]


def _ascii_common() -> dict[int, str]:
    table: dict[int, str] = dict(_ASCII_PUNCT)
    for i in range(10):
        table[0x30 + i] = _DIGIT_NAMES[i]
    for i in range(26):
        table[0x41 + i] = chr(0x41 + i)          # A..Z
        table[0x61 + i] = chr(0x61 + i)          # a..z
    return table


def build_base_encodings() -> dict[str, dict[int, str]]:
    """Return ``{table_name: {code: glyph_name}}`` for the supported bases.

    Seed only (see module docstring): the ASCII range plus the codes where
    WinAnsi and Standard diverge. Codes absent from a table render as
    ``.notdef`` (the empty string in the generated array).
    """
    common = _ascii_common()

    standard = dict(common)
    # Adobe StandardEncoding divergences from ASCII typography.
    standard[0x27] = "quoteright"
    standard[0x60] = "quoteleft"

    win_ansi = dict(common)
    win_ansi[0x27] = "quotesingle"
    win_ansi[0x60] = "grave"
    # A couple of WinAnsi upper-half entries the tests rely on; the rest of the
    # Latin-1 range is a follow-up.
    win_ansi[0xA9] = "copyright"
    win_ansi[0xAE] = "registered"

    mac_roman = dict(common)
    mac_roman[0x27] = "quotesingle"
    mac_roman[0x60] = "grave"

    pdf_doc = dict(win_ansi)

    return {
        "standard_encoding": standard,
        "win_ansi_encoding": win_ansi,
        "mac_roman_encoding": mac_roman,
        "pdf_doc_encoding": pdf_doc,
    }


# Adobe Glyph List seed: glyph name -> list of Unicode code points. Names that
# follow the algorithmic ``uniXXXX`` / ``uXXXXXX`` forms are resolved at runtime
# and are NOT listed here. The full list is loaded from ``glyphlist.txt``.
def build_agl_seed() -> dict[str, list[int]]:
    agl: dict[str, list[int]] = {}
    for code, name in _ascii_common().items():
        agl[name] = [code]
    agl.update({
        "quoteright": [0x2019],
        "quoteleft": [0x2018],
        "quotesingle": [0x0027],
        "grave": [0x0060],
        "copyright": [0x00A9],
        "registered": [0x00AE],
        "bullet": [0x2022],
        "endash": [0x2013],
        "emdash": [0x2014],
        "fi": [0x0066, 0x0069],   # ligature -> "fi"
        "fl": [0x0066, 0x006C],   # ligature -> "fl"
    })
    return agl


def load_agl(path: str) -> dict[str, list[int]]:
    """Parse Adobe ``glyphlist.txt`` (``name;HHHH HHHH`` lines)."""
    agl: dict[str, list[int]] = {}
    with open(path, encoding="ascii") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name, _, codes = line.partition(";")
            agl[name] = [int(code, 16) for code in codes.split()]
    return agl


# --- Emit -------------------------------------------------------------------

_AUTOGEN = (
    "// Auto-generated by tools/pdf/generate_encoding_data.py — DO NOT EDIT.\n"
    "// Regenerate with the Adobe Glyph List to populate the full tables; see\n"
    "// the script header and pdf/AGENTS.md (stage 1.2).\n"
)


def _cpp_u16(code_points: list[int]) -> str:
    return "u\"" + "".join(f"\\u{cp:04X}" for cp in code_points) + "\""


def emit_hpp() -> str:
    out = [_AUTOGEN, "", "#pragma once", "",
           "#include <array>", "#include <string_view>", "#include <utility>",
           "", "namespace odr::internal::pdf::encoding_data {", ""]
    for name in ("standard_encoding", "win_ansi_encoding",
                 "mac_roman_encoding", "pdf_doc_encoding"):
        out.append(f"extern const std::array<std::string_view, 256> {name};")
    out += [
        "",
        "// Adobe Glyph List, sorted by glyph name for binary search; the value is",
        "// the UTF-16 sequence the name maps to.",
        "extern const std::array<std::pair<std::string_view, std::u16string_view>,",
        "                        adobe_glyph_list_size>",
        "    adobe_glyph_list;",
        "",
        "} // namespace odr::internal::pdf::encoding_data",
        "",
    ]
    return "\n".join(out)


def emit_cpp(bases: dict[str, dict[int, str]],
             agl: dict[str, list[int]]) -> tuple[str, int]:
    out = [_AUTOGEN, "",
           "#include <odr/internal/pdf/pdf_encoding_data.hpp>", "",
           "namespace odr::internal::pdf::encoding_data {", ""]

    for table_name, mapping in bases.items():
        out.append(
            f"const std::array<std::string_view, 256> {table_name} = {{{{")
        for code in range(256):
            glyph = mapping.get(code, "")
            comment = f"  // 0x{code:02X}"
            out.append(f'    "{glyph}",{comment}')
        out += ["}};", ""]

    entries = sorted(agl.items())
    for name, _ in entries:
        assert ";" not in name and '"' not in name, name
    out.append(
        "const std::array<std::pair<std::string_view, std::u16string_view>,")
    out.append("           adobe_glyph_list_size>")
    out.append("    adobe_glyph_list = {{")
    for name, code_points in entries:
        out.append(f'    {{"{name}", {_cpp_u16(code_points)}}},')
    out += ["}};", "",
            "} // namespace odr::internal::pdf::encoding_data", ""]
    return "\n".join(out), len(entries)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--agl", help="path to Adobe glyphlist.txt")
    parser.add_argument(
        "--out",
        default=os.path.join("src", "odr", "internal", "pdf"),
        help="output directory for pdf_encoding_data.{hpp,cpp}")
    args = parser.parse_args()

    bases = build_base_encodings()
    agl = load_agl(args.agl) if args.agl else build_agl_seed()

    cpp, count = emit_cpp(bases, agl)
    # The size constant lives in the header so the array type is complete there.
    hpp = emit_hpp().replace(
        "namespace odr::internal::pdf::encoding_data {\n",
        "namespace odr::internal::pdf::encoding_data {\n\n"
        f"inline constexpr std::size_t adobe_glyph_list_size = {count};\n",
        1).replace("#include <utility>",
                   "#include <utility>\n#include <cstddef>")

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "pdf_encoding_data.hpp"), "w") as handle:
        handle.write(hpp)
    with open(os.path.join(args.out, "pdf_encoding_data.cpp"), "w") as handle:
        handle.write(cpp)

    src = args.agl if args.agl else "embedded seed"
    print(f"generated pdf_encoding_data.{{hpp,cpp}} "
          f"({count} AGL entries from {src})")


if __name__ == "__main__":
    main()
