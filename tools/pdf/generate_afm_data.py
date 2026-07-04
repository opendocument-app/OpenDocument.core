#!/usr/bin/env python3
"""Generate the standard-14 AFM glyph-metric tables for the PDF module.

A non-embedded font (no ``/FontFile*``) is substituted, not rendered: the glyph
shapes come from the browser's fallback font, but placement still needs the
*intended* advance widths. The base-14 fonts (Helvetica/Times/Courier families +
Symbol/ZapfDingbats) usually ship no ``/Widths`` array, so their widths come from
Adobe's Core-14 AFM metric files, baked here into committed C++
(``pdf_afm_data.{hpp,cpp}``). The build only compiles the output; there is no
build-time dependency on this script. Re-run it whenever the source data or the
font list changes:

    python3 tools/pdf/generate_afm_data.py

Source data -- the 14 Adobe Core AFM files -- are fetched from a pinned Apache
PDFBox tag (Apache-2.0; the files themselves are Adobe's, redistributed by
PDFBox) into the git-ignored ``afm/`` cache next to this script and reused on
later runs, the same way ``generate_encoding_data.py`` fetches the AGL. See
``tools/pdf/THIRD_PARTY_LICENSES.md``.

Each AFM lists per-glyph metrics as ``C <code> ; WX <width> ; N <name> ; ...``
lines (ISO 32000-1 references the Adobe AFM spec). ``code`` is the glyph's slot
in the font's built-in encoding, ``-1`` for glyphs outside it (still carrying a
name and width -- reachable through a PDF ``/Differences`` array). We key widths
by glyph *name* (the encoding-independent path, used with a PDF ``/Encoding``)
and additionally keep the built-in ``code -> width`` table (the fallback when a
font carries no ``/Encoding`` at all -- notably Symbol/ZapfDingbats).
"""

from __future__ import annotations

import argparse
import os
import sys
import urllib.request

_DATA_DIR = os.path.dirname(os.path.abspath(__file__))
_AFM_CACHE = os.path.join(_DATA_DIR, "afm")

# Pinned PDFBox tag; its resources bundle Adobe's Core-14 AFM files verbatim.
_PDFBOX_TAG = "3.0.3"
_AFM_URL = (
    "https://raw.githubusercontent.com/apache/pdfbox/"
    f"{_PDFBOX_TAG}/pdfbox/src/main/resources/org/apache/pdfbox/resources/afm"
)

# The 14 fonts, in the order the C++ `StandardFont` enum expects (see pdf_afm.hpp).
_FONTS = (
    "Helvetica",
    "Helvetica-Bold",
    "Helvetica-Oblique",
    "Helvetica-BoldOblique",
    "Times-Roman",
    "Times-Bold",
    "Times-Italic",
    "Times-BoldItalic",
    "Courier",
    "Courier-Bold",
    "Courier-Oblique",
    "Courier-BoldOblique",
    "Symbol",
    "ZapfDingbats",
)


# --- Fetching ---------------------------------------------------------------


def ensure_afm(font: str) -> str:
    """Download the pinned AFM for `font` if absent; return its path."""
    os.makedirs(_AFM_CACHE, exist_ok=True)
    path = os.path.join(_AFM_CACHE, f"{font}.afm")
    if os.path.isfile(path):
        return path
    url = f"{_AFM_URL}/{font}.afm"
    print(f"downloading {url}", file=sys.stderr)
    with urllib.request.urlopen(url) as response:  # noqa: S310 (pinned)
        payload = response.read()
    with open(path, "wb") as out:
        out.write(payload)
    return path


# --- Parsing ----------------------------------------------------------------


class Afm:
    """One parsed AFM: name->width, the 256-entry built-in code->width table,
    and the header ascent/descent/cap-height metrics (glyph space, /1000)."""

    def __init__(self) -> None:
        self.widths: dict[str, int] = {}
        self.code_widths: list[int] = [-1] * 256
        self.ascender = 0
        self.descender = 0
        self.cap_height = 0
        self.bbox_ymax = 0


def parse_afm(path: str) -> Afm:
    afm = Afm()
    with open(path, encoding="latin-1") as handle:
        for line in handle:
            line = line.strip()
            if line.startswith("C "):
                # `C <code> ; WX <w> ; N <name> ; B ...`
                fields = [f.strip() for f in line.split(";")]
                code = width = None
                name = None
                for field in fields:
                    parts = field.split()
                    if not parts:
                        continue
                    if parts[0] == "C":
                        code = int(parts[1])
                    elif parts[0] == "WX":
                        width = int(round(float(parts[1])))
                    elif parts[0] == "N":
                        name = parts[1]
                if name is not None and width is not None:
                    afm.widths[name] = width
                    if code is not None and 0 <= code <= 255:
                        afm.code_widths[code] = width
            elif line.startswith("Ascender "):
                afm.ascender = int(round(float(line.split()[1])))
            elif line.startswith("Descender "):
                afm.descender = int(round(float(line.split()[1])))
            elif line.startswith("CapHeight "):
                afm.cap_height = int(round(float(line.split()[1])))
            elif line.startswith("FontBBox "):
                afm.bbox_ymax = int(round(float(line.split()[4])))
    # Symbol/ZapfDingbats carry no Ascender/Descender: fall back to the FontBBox
    # so the substitute still gets a plausible baseline shift.
    if afm.ascender == 0:
        afm.ascender = afm.bbox_ymax
    return afm


# --- Emit -------------------------------------------------------------------

_AUTOGEN = (
    "// Auto-generated by tools/pdf/generate_afm_data.py -- DO NOT EDIT.\n"
    "// Regenerate by rerunning the script; see its header for the source data.\n"
    "// Adobe Core-14 AFM metrics, redistributed by Apache PDFBox (Apache-2.0);\n"
    "// see tools/pdf/THIRD_PARTY_LICENSES.md.\n"
    "\n"
    "// clang-format off"
)


def _ident(font: str) -> str:
    return font.lower().replace("-", "_")


def emit_hpp() -> str:
    out = [
        _AUTOGEN,
        "",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace odr::internal::pdf::afm_data {",
        "",
        "/// One glyph's advance width (glyph space, 1/1000 em), keyed by name.",
        "struct GlyphWidth {",
        "  std::string_view name;",
        "  std::int16_t width;",
        "};",
        "",
        "/// One standard-14 font: its name->width table (sorted by name for a",
        "/// binary search), the built-in encoding's code->width table (-1 where",
        "/// the slot is empty), and header metrics (glyph space, 1/1000 em).",
        "struct FontMetrics {",
        "  std::string_view font_name;",
        "  std::int16_t ascender;",
        "  std::int16_t descender;",
        "  std::int16_t cap_height;",
        "  const GlyphWidth *glyphs;",
        "  std::size_t glyph_count;",
        "  const std::int16_t *code_widths; // 256 entries; -1 == absent",
        "};",
        "",
        f"inline constexpr std::size_t font_count = {len(_FONTS)};",
        "",
        "extern const std::array<FontMetrics, font_count> fonts;",
        "",
        "} // namespace odr::internal::pdf::afm_data",
        "",
    ]
    return "\n".join(out)


def emit_cpp(afms: list[tuple[str, Afm]]) -> str:
    out = [
        _AUTOGEN,
        "",
        "#include <odr/internal/pdf/pdf_afm_data.hpp>",
        "",
        "namespace odr::internal::pdf::afm_data {",
        "namespace {",
        "",
    ]

    for font, afm in afms:
        ident = _ident(font)
        out.append(f"constexpr GlyphWidth glyphs_{ident}[] = {{")
        for name, width in sorted(afm.widths.items()):
            assert '"' not in name and "\\" not in name, name
            out.append(f'    {{"{name}", {width}}},')
        out += ["};", ""]

        out.append(f"constexpr std::int16_t code_widths_{ident}[256] = {{")
        row: list[str] = []
        for code in range(256):
            row.append(str(afm.code_widths[code]))
            if len(row) == 16:
                out.append("    " + ", ".join(row) + ",")
                row = []
        out += ["};", ""]

    out += ["} // namespace", ""]

    out.append("const std::array<FontMetrics, font_count> fonts = {{")
    for font, afm in afms:
        ident = _ident(font)
        out.append(
            f'    {{"{font}", {afm.ascender}, {afm.descender}, '
            f"{afm.cap_height}, glyphs_{ident}, "
            f"std::size(glyphs_{ident}), code_widths_{ident}}},"
        )
    out += ["}};", "", "} // namespace odr::internal::pdf::afm_data", ""]
    return "\n".join(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        default=os.path.join("src", "odr", "internal", "pdf"),
        help="output directory for pdf_afm_data.{hpp,cpp}",
    )
    args = parser.parse_args()

    afms = [(font, parse_afm(ensure_afm(font))) for font in _FONTS]

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "pdf_afm_data.hpp"), "w") as handle:
        handle.write(emit_hpp())
    with open(os.path.join(args.out, "pdf_afm_data.cpp"), "w") as handle:
        handle.write(emit_cpp(afms))

    glyphs = sum(len(afm.widths) for _, afm in afms)
    print(f"generated pdf_afm_data.{{hpp,cpp}} ({len(afms)} fonts, {glyphs} glyphs)")


if __name__ == "__main__":
    main()
