#!/usr/bin/env python3
"""Generate the CFF Standard Strings table for the font module.

The output is committed C++ source (``cff_standard_strings.{hpp,cpp}``); the
build only compiles it, so there is no build-time dependency on this script.
Re-run it if the (fixed) spec data ever needs to change:

    python3 tools/font/generate_cff_standard_strings.py

The 391 CFF Standard Strings (SID 0..390) are fixed spec data from Adobe
Technical Note #5176 ("The Compact Font Format Specification"), Appendix A.
They are vendored here (no canonical download), the same way the base-encoding
tables are vendored next to ``tools/pdf/generate_encoding_data.py``. A CFF
charset references these SIDs for the common glyph names; only names beyond SID
390 live in a font's String INDEX.
"""

from __future__ import annotations

import argparse
import os

# CFF Standard Strings, SID 0..390 (Adobe TN #5176, Appendix A). Order is
# significant: the index *is* the SID.
STANDARD_STRINGS = [
    ".notdef", "space", "exclam", "quotedbl", "numbersign", "dollar",
    "percent", "ampersand", "quoteright", "parenleft", "parenright",
    "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one",
    "two", "three", "four", "five", "six", "seven", "eight", "nine", "colon",
    "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C",
    "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R",
    "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash",
    "bracketright", "asciicircum", "underscore", "quoteleft", "a", "b", "c",
    "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright",
    "asciitilde", "exclamdown", "cent", "sterling", "fraction", "yen",
    "florin", "section", "currency", "quotesingle", "quotedblleft",
    "guillemotleft", "guilsinglleft", "guilsinglright", "fi", "fl", "endash",
    "dagger", "daggerdbl", "periodcentered", "paragraph", "bullet",
    "quotesinglbase", "quotedblbase", "quotedblright", "guillemotright",
    "ellipsis", "perthousand", "questiondown", "grave", "acute", "circumflex",
    "tilde", "macron", "breve", "dotaccent", "dieresis", "ring", "cedilla",
    "hungarumlaut", "ogonek", "caron", "emdash", "AE", "ordfeminine",
    "Lslash", "Oslash", "OE", "ordmasculine", "ae", "dotlessi", "lslash",
    "oslash", "oe", "germandbls", "onesuperior", "logicalnot", "mu",
    "trademark", "Eth", "onehalf", "plusminus", "Thorn", "onequarter",
    "divide", "brokenbar", "degree", "thorn", "threequarters", "twosuperior",
    "registered", "minus", "eth", "multiply", "threesuperior", "copyright",
    "Aacute", "Acircumflex", "Adieresis", "Agrave", "Aring", "Atilde",
    "Ccedilla", "Eacute", "Ecircumflex", "Edieresis", "Egrave", "Iacute",
    "Icircumflex", "Idieresis", "Igrave", "Ntilde", "Oacute", "Ocircumflex",
    "Odieresis", "Ograve", "Otilde", "Scaron", "Uacute", "Ucircumflex",
    "Udieresis", "Ugrave", "Yacute", "Ydieresis", "Zcaron", "aacute",
    "acircumflex", "adieresis", "agrave", "aring", "atilde", "ccedilla",
    "eacute", "ecircumflex", "edieresis", "egrave", "iacute", "icircumflex",
    "idieresis", "igrave", "ntilde", "oacute", "ocircumflex", "odieresis",
    "ograve", "otilde", "scaron", "uacute", "ucircumflex", "udieresis",
    "ugrave", "yacute", "ydieresis", "zcaron", "exclamsmall",
    "Hungarumlautsmall", "dollaroldstyle", "dollarsuperior", "ampersandsmall",
    "Acutesmall", "parenleftsuperior", "parenrightsuperior", "twodotenleader",
    "onedotenleader", "zerooldstyle", "oneoldstyle", "twooldstyle",
    "threeoldstyle", "fouroldstyle", "fiveoldstyle", "sixoldstyle",
    "sevenoldstyle", "eightoldstyle", "nineoldstyle", "commasuperior",
    "threequartersemdash", "periodsuperior", "questionsmall", "asuperior",
    "bsuperior", "centsuperior", "dsuperior", "esuperior", "isuperior",
    "lsuperior", "msuperior", "nsuperior", "osuperior", "rsuperior",
    "ssuperior", "tsuperior", "ff", "ffi", "ffl", "parenleftinferior",
    "parenrightinferior", "Circumflexsmall", "hyphensuperior", "Gravesmall",
    "Asmall", "Bsmall", "Csmall", "Dsmall", "Esmall", "Fsmall", "Gsmall",
    "Hsmall", "Ismall", "Jsmall", "Ksmall", "Lsmall", "Msmall", "Nsmall",
    "Osmall", "Psmall", "Qsmall", "Rsmall", "Ssmall", "Tsmall", "Usmall",
    "Vsmall", "Wsmall", "Xsmall", "Ysmall", "Zsmall", "colonmonetary",
    "onefitted", "rupiah", "Tildesmall", "exclamdownsmall", "centoldstyle",
    "Lslashsmall", "Scaronsmall", "Zcaronsmall", "Dieresissmall",
    "Brevesmall", "Caronsmall", "Dotaccentsmall", "Macronsmall", "figuredash",
    "hypheninferior", "Ogoneksmall", "Ringsmall", "Cedillasmall",
    "questiondownsmall", "oneeighth", "threeeighths", "fiveeighths",
    "seveneighths", "onethird", "twothirds", "zerosuperior", "foursuperior",
    "fivesuperior", "sixsuperior", "sevensuperior", "eightsuperior",
    "ninesuperior", "zeroinferior", "oneinferior", "twoinferior",
    "threeinferior", "fourinferior", "fiveinferior", "sixinferior",
    "seveninferior", "eightinferior", "nineinferior", "centinferior",
    "dollarinferior", "periodinferior", "commainferior", "Agravesmall",
    "Aacutesmall", "Acircumflexsmall", "Atildesmall", "Adieresissmall",
    "Aringsmall", "AEsmall", "Ccedillasmall", "Egravesmall", "Eacutesmall",
    "Ecircumflexsmall", "Edieresissmall", "Igravesmall", "Iacutesmall",
    "Icircumflexsmall", "Idieresissmall", "Ethsmall", "Ntildesmall",
    "Ogravesmall", "Oacutesmall", "Ocircumflexsmall", "Otildesmall",
    "Odieresissmall", "OEsmall", "Oslashsmall", "Ugravesmall", "Uacutesmall",
    "Ucircumflexsmall", "Udieresissmall", "Yacutesmall", "Thornsmall",
    "Ydieresissmall", "001.000", "001.001", "001.002", "001.003", "Black",
    "Bold", "Book", "Light", "Medium", "Regular", "Roman", "Semibold",
]

_BANNER = (
    "// Auto-generated by tools/font/generate_cff_standard_strings.py -- DO NOT EDIT.\n"
    "// Regenerate by rerunning the script; see its header for the source data.\n"
    "// CFF Standard Strings: Adobe Technical Note #5176, Appendix A.\n"
    "\n// clang-format off\n\n"
)


def emit_hpp(size: int) -> str:
    lines = [
        _BANNER,
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <string_view>",
        "",
        "namespace odr::internal::font::cff {",
        "",
        f"inline constexpr std::size_t cff_standard_strings_size = {size};",
        "",
        "extern const std::array<std::string_view, cff_standard_strings_size>",
        "    cff_standard_strings;",
        "",
        "} // namespace odr::internal::font::cff",
        "",
    ]
    return "\n".join(lines)


def emit_cpp(strings: list[str]) -> str:
    out = [
        _BANNER,
        "#include <odr/internal/font/cff_standard_strings.hpp>",
        "",
        "namespace odr::internal::font::cff {",
        "",
        "const std::array<std::string_view, cff_standard_strings_size>",
        "    cff_standard_strings = {{",
    ]
    for name in strings:
        out.append(f'    "{name}",')
    out += ["}};", "", "} // namespace odr::internal::font::cff", ""]
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..", "..", "src", "odr", "internal", "font"),
        help="output directory for the generated C++ files",
    )
    args = parser.parse_args()

    assert len(STANDARD_STRINGS) == 391, len(STANDARD_STRINGS)
    assert len(set(STANDARD_STRINGS)) == 391, "duplicate standard string"

    out_dir = os.path.abspath(args.out)
    with open(os.path.join(out_dir, "cff_standard_strings.hpp"), "w") as handle:
        handle.write(emit_hpp(len(STANDARD_STRINGS)))
    with open(os.path.join(out_dir, "cff_standard_strings.cpp"), "w") as handle:
        handle.write(emit_cpp(STANDARD_STRINGS))


if __name__ == "__main__":
    main()
