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
(https://raw.githubusercontent.com/adobe-type-tools/agl-aglfn/master/glyphlist.txt).
When ``--agl`` is omitted the script falls back to the small SEED list embedded
below so the module still compiles offline; pass the real file for the full
~4,300-entry AGL.

The three base-encoding tables (ISO 32000-1 Annex D — StandardEncoding,
WinAnsiEncoding, MacRomanEncoding) are full and embedded below; only the AGL is
an external input.
"""

from __future__ import annotations

import argparse
import os

# --- Base encodings (ISO 32000-1 Annex D) -----------------------------------

# The canonical 256-entry glyph-name tables; "" is .notdef.
_BASE_ENCODINGS: dict[str, list[str]] = {
  "standard_encoding": [
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quoteright",
    "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
    "at", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
    "quoteleft", "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "exclamdown", "cent", "sterling", "fraction", "yen", "florin", "section",
    "currency", "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft", "guilsinglright", "fi", "fl",
    "", "endash", "dagger", "daggerdbl", "periodcentered", "", "paragraph", "bullet",
    "quotesinglbase", "quotedblbase", "quotedblright", "guillemotright", "ellipsis", "perthousand", "", "questiondown",
    "", "grave", "acute", "circumflex", "tilde", "macron", "breve", "dotaccent",
    "dieresis", "", "ring", "cedilla", "", "hungarumlaut", "ogonek", "caron",
    "emdash", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "AE", "", "ordfeminine", "", "", "", "",
    "Lslash", "Oslash", "OE", "ordmasculine", "", "", "", "",
    "", "ae", "", "", "", "dotlessi", "", "",
    "lslash", "oslash", "oe", "germandbls", "", "", "", "",
  ],
  "win_ansi_encoding": [
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle",
    "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
    "at", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
    "grave", "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "bullet",
    "Euro", "bullet", "quotesinglbase", "florin", "quotedblbase", "ellipsis", "dagger", "daggerdbl",
    "circumflex", "perthousand", "Scaron", "guilsinglleft", "OE", "bullet", "Zcaron", "bullet",
    "bullet", "quoteleft", "quoteright", "quotedblleft", "quotedblright", "bullet", "endash", "emdash",
    "tilde", "trademark", "scaron", "guilsinglright", "oe", "bullet", "zcaron", "Ydieresis",
    "space", "exclamdown", "cent", "sterling", "currency", "yen", "brokenbar", "section",
    "dieresis", "copyright", "ordfeminine", "guillemotleft", "logicalnot", "hyphen", "registered", "macron",
    "degree", "plusminus", "twosuperior", "threesuperior", "acute", "mu", "paragraph", "periodcentered",
    "cedilla", "onesuperior", "ordmasculine", "guillemotright", "onequarter", "onehalf", "threequarters", "questiondown",
    "Agrave", "Aacute", "Acircumflex", "Atilde", "Adieresis", "Aring", "AE", "Ccedilla",
    "Egrave", "Eacute", "Ecircumflex", "Edieresis", "Igrave", "Iacute", "Icircumflex", "Idieresis",
    "Eth", "Ntilde", "Ograve", "Oacute", "Ocircumflex", "Otilde", "Odieresis", "multiply",
    "Oslash", "Ugrave", "Uacute", "Ucircumflex", "Udieresis", "Yacute", "Thorn", "germandbls",
    "agrave", "aacute", "acircumflex", "atilde", "adieresis", "aring", "ae", "ccedilla",
    "egrave", "eacute", "ecircumflex", "edieresis", "igrave", "iacute", "icircumflex", "idieresis",
    "eth", "ntilde", "ograve", "oacute", "ocircumflex", "otilde", "odieresis", "divide",
    "oslash", "ugrave", "uacute", "ucircumflex", "udieresis", "yacute", "thorn", "ydieresis",
  ],
  "mac_roman_encoding": [
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle",
    "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three", "four", "five", "six", "seven",
    "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question",
    "at", "A", "B", "C", "D", "E", "F", "G",
    "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W",
    "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
    "grave", "a", "b", "c", "d", "e", "f", "g",
    "h", "i", "j", "k", "l", "m", "n", "o",
    "p", "q", "r", "s", "t", "u", "v", "w",
    "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "",
    "Adieresis", "Aring", "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute",
    "agrave", "acircumflex", "adieresis", "atilde", "aring", "ccedilla", "eacute", "egrave",
    "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis", "ntilde", "oacute",
    "ograve", "ocircumflex", "odieresis", "otilde", "uacute", "ugrave", "ucircumflex", "udieresis",
    "dagger", "degree", "cent", "sterling", "section", "bullet", "paragraph", "germandbls",
    "registered", "copyright", "trademark", "acute", "dieresis", "notequal", "AE", "Oslash",
    "infinity", "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff", "summation",
    "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae", "oslash",
    "questiondown", "exclamdown", "logicalnot", "radical", "florin", "approxequal", "Delta", "guillemotleft",
    "guillemotright", "ellipsis", "space", "Agrave", "Atilde", "Otilde", "OE", "oe",
    "endash", "emdash", "quotedblleft", "quotedblright", "quoteleft", "quoteright", "divide", "lozenge",
    "ydieresis", "Ydieresis", "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl",
    "daggerdbl", "periodcentered", "quotesinglbase", "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute",
    "Edieresis", "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex",
    "apple", "Ograve", "Uacute", "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde",
    "macron", "breve", "dotaccent", "ring", "cedilla", "hungarumlaut", "ogonek", "caron",
  ],
}


def build_base_encodings() -> dict[str, list[str]]:
    for name, table in _BASE_ENCODINGS.items():
        assert len(table) == 256, (name, len(table))
    return _BASE_ENCODINGS


# Adobe Glyph List seed (offline fallback): glyph name -> list of code points.
# The full list is loaded from ``glyphlist.txt``. Names that follow the
# algorithmic ``uniXXXX`` / ``uXXXXXX`` forms are resolved at runtime and are not
# listed here.
def build_agl_seed() -> dict[str, list[int]]:
    agl: dict[str, list[int]] = {}
    seen: set[str] = set()
    for table in _BASE_ENCODINGS.values():
        seen.update(name for name in table if name)
    common = {
        "space": [0x20], "exclam": [0x21], "quotedbl": [0x22],
        "numbersign": [0x23], "dollar": [0x24], "percent": [0x25],
        "ampersand": [0x26], "quotesingle": [0x27], "parenleft": [0x28],
        "parenright": [0x29], "asterisk": [0x2A], "plus": [0x2B],
        "comma": [0x2C], "hyphen": [0x2D], "period": [0x2E], "slash": [0x2F],
        "colon": [0x3A], "semicolon": [0x3B], "less": [0x3C], "equal": [0x3D],
        "greater": [0x3E], "question": [0x3F], "at": [0x40],
        "bracketleft": [0x5B], "backslash": [0x5C], "bracketright": [0x5D],
        "asciicircum": [0x5E], "underscore": [0x5F], "braceleft": [0x7B],
        "bar": [0x7C], "braceright": [0x7D], "asciitilde": [0x7E],
        "quoteright": [0x2019], "quoteleft": [0x2018], "grave": [0x60],
        "copyright": [0x00A9], "registered": [0x00AE], "bullet": [0x2022],
        "endash": [0x2013], "emdash": [0x2014],
        "fi": [0x0066, 0x0069], "fl": [0x0066, 0x006C],
    }
    digits = ["zero", "one", "two", "three", "four",
              "five", "six", "seven", "eight", "nine"]
    for i, name in enumerate(digits):
        common[name] = [0x30 + i]
    for i in range(26):
        common[chr(0x41 + i)] = [0x41 + i]
        common[chr(0x61 + i)] = [0x61 + i]
    for name, code_points in common.items():
        agl[name] = code_points
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
    "// Regenerate with the Adobe Glyph List (--agl glyphlist.txt) to refresh;\n"
    "// see the script header and pdf/AGENTS.md (stage 1.2).\n"
    "// clang-format off\n"
)

_ENCODING_NAMES = ("standard_encoding", "win_ansi_encoding", "mac_roman_encoding")


def _cpp_u16(code_points: list[int]) -> str:
    return "u\"" + "".join(f"\\u{cp:04X}" for cp in code_points) + "\""


def emit_hpp(agl_size: int) -> str:
    out = [_AUTOGEN, "", "#pragma once", "",
           "#include <array>", "#include <cstddef>", "#include <string_view>",
           "#include <utility>", "",
           "namespace odr::internal::pdf::encoding_data {", "",
           f"inline constexpr std::size_t adobe_glyph_list_size = {agl_size};",
           ""]
    for name in _ENCODING_NAMES:
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


def emit_cpp(bases: dict[str, list[str]],
             agl: dict[str, list[int]]) -> tuple[str, int]:
    out = [_AUTOGEN, "",
           "#include <odr/internal/pdf/pdf_encoding_data.hpp>", "",
           "namespace odr::internal::pdf::encoding_data {", ""]

    for table_name in _ENCODING_NAMES:
        mapping = bases[table_name]
        out.append(
            f"const std::array<std::string_view, 256> {table_name} = {{{{")
        for code in range(256):
            glyph = mapping[code]
            out.append(f'    "{glyph}", // 0x{code:02X}')
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
    hpp = emit_hpp(count)

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
