#!/usr/bin/env python3
"""Generate the predefined-CMap tables for composite (Type0/CID) PDF fonts.

The output is committed C++ source (``pdf_cid_data.{hpp,cpp}``); the build only
compiles it, so there is no build-time dependency on this script. Re-run it to
refresh the data:

    python3 tools/pdf/generate_cid_data.py

Input data is Adobe's CMap resources
(https://github.com/adobe-type-tools/cmap-resources, pinned by commit below,
licensed BSD-3-Clause -- see ``tools/pdf/THIRD_PARTY_LICENSES.md``). The archive
is large (~14 MB unpacked), so it is **not** vendored: the script downloads it
into ``tools/pdf/cmap-resources/`` (git-ignored) on first run and reuses it
afterwards.

What the chain needs (a composite font's character code -> Unicode):

  * ``code -> CID``: the predefined CMap the PDF names (e.g. ``90ms-RKSJ-H``).
  * ``CID -> Unicode``: per character collection (Adobe-Japan1, -GB1, -CNS1,
    -Korea1, -KR), obtained by inverting that collection's ``Uni*-UTF32-H``
    (or ``-UTF16-H`` / ``-UCS2-H``) CMap.

The ``Uni*`` CMaps themselves are **not** emitted: their character codes already
*are* Unicode (UTF-16BE / UTF-32BE), so the C++ runtime decodes them directly
(see ``pdf_cid.cpp``). Only the legacy named CMaps carry data tables here.

Tables are block-encoded as plain integer range arrays (no compression);
identical tables (e.g. an ``-H``/``-V`` pair) are de-duplicated by sharing
offsets.
"""

from __future__ import annotations

import io
import os
import sys
import tarfile
import urllib.request
from collections.abc import Iterator

_HERE = os.path.dirname(os.path.abspath(__file__))
_OUT_DIR = os.path.normpath(os.path.join(_HERE, "../../src/odr/internal/pdf"))

# Pinned commit of adobe-type-tools/cmap-resources.
_COMMIT = "f5cf3bca7fdfeaceb77aa82847e974f2306c20b4"
_ARCHIVE_URL = (
    f"https://github.com/adobe-type-tools/cmap-resources/archive/{_COMMIT}.tar.gz"
)
_DATA_DIR = os.path.join(_HERE, "cmap-resources")

# Character collections to cover, in output order. Each is the directory name in
# the archive; the registry/ordering pair identifies it from a PDF's
# /CIDSystemInfo, and the legacy CMaps live under "<dir>/CMap/".
_COLLECTIONS = (
    ("Adobe-Japan1-7", "Adobe", "Japan1"),
    ("Adobe-GB1-6", "Adobe", "GB1"),
    ("Adobe-CNS1-7", "Adobe", "CNS1"),
    ("Adobe-Korea1-2", "Adobe", "Korea1"),
    ("Adobe-KR-9", "Adobe", "KR"),
)


# --- Fetching ---------------------------------------------------------------


def ensure_data() -> None:
    """Download and unpack the pinned cmap-resources archive if it is absent."""
    if os.path.isdir(_DATA_DIR) and os.listdir(_DATA_DIR):
        return
    print(f"downloading {_ARCHIVE_URL}", file=sys.stderr)
    with urllib.request.urlopen(_ARCHIVE_URL) as response:  # noqa: S310 (pinned)
        payload = response.read()
    os.makedirs(_DATA_DIR, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:gz") as archive:
        prefix = f"cmap-resources-{_COMMIT}/"
        for member in archive.getmembers():
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            rel = member.name[len(prefix) :]
            target = os.path.join(_DATA_DIR, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            src = archive.extractfile(member)
            assert src is not None
            with open(target, "wb") as out:
                out.write(src.read())
    print(f"unpacked into {_DATA_DIR}", file=sys.stderr)


# --- CMap parsing (Adobe CMap syntax) ---------------------------------------


class CMap:
    """A parsed Adobe CMap: codespace ranges plus code -> CID mappings."""

    def __init__(self) -> None:
        # (low, high, width) -- width in bytes, low/high as ints.
        self.codespace: list[tuple[int, int, int]] = []
        # (code_low, code_high, cid_low, width) -- a contiguous CID run.
        self.cid_ranges: list[tuple[int, int, int, int]] = []

    def cid_map(self) -> dict[int, int]:
        """Flatten to a {code: cid} dict (used for inversion)."""
        result: dict[int, int] = {}
        for code_low, code_high, cid_low, _width in self.cid_ranges:
            for offset in range(code_high - code_low + 1):
                result[code_low + offset] = cid_low + offset
        return result


def _tokens(text: str) -> Iterator[str]:
    for raw in text.replace("\r", "\n").split("\n"):
        line = raw.split("%", 1)[0]
        yield from line.split()


def _hex_to_int_width(token: str) -> tuple[int, int]:
    """`<00a1>` -> (0x00a1, width-in-bytes)."""
    assert token.startswith("<") and token.endswith(">"), token
    digits = token[1:-1]
    return int(digits, 16), len(digits) // 2


def parse_cmap(directory: str, name: str, _seen: set[str] | None = None) -> CMap:
    """Parse a CMap file under ``directory``, resolving ``usecmap`` references."""
    seen = _seen if _seen is not None else set()
    cmap = CMap()
    if name in seen:
        return cmap
    seen.add(name)

    with open(os.path.join(directory, name), encoding="latin-1") as handle:
        tokens = list(_tokens(handle.read()))

    i = 0
    n = len(tokens)
    while i < n:
        token = tokens[i]
        if token == "usecmap":
            parent = tokens[i - 1].lstrip("/")
            base = parse_cmap(directory, parent, seen)
            cmap.codespace.extend(base.codespace)
            cmap.cid_ranges.extend(base.cid_ranges)
            i += 1
        elif token == "begincodespacerange":
            i += 1
            while i < n and tokens[i] != "endcodespacerange":
                low, width = _hex_to_int_width(tokens[i])
                high, _ = _hex_to_int_width(tokens[i + 1])
                cmap.codespace.append((low, high, width))
                i += 2
        elif token == "begincidrange":
            i += 1
            while i < n and tokens[i] != "endcidrange":
                low, width = _hex_to_int_width(tokens[i])
                high, _ = _hex_to_int_width(tokens[i + 1])
                cid = int(tokens[i + 2])
                cmap.cid_ranges.append((low, high, cid, width))
                i += 3
        elif token == "begincidchar":
            i += 1
            while i < n and tokens[i] != "endcidchar":
                code, width = _hex_to_int_width(tokens[i])
                cid = int(tokens[i + 1])
                cmap.cid_ranges.append((code, code, cid, width))
                i += 2
        else:
            i += 1
    return cmap


# --- CID -> Unicode (invert a Uni* CMap) ------------------------------------


def _find_unicode_cmap(cmap_dir: str) -> str:
    """The best Unicode CMap for a collection: UTF32 > UTF16 > UCS2, "-H"."""
    names = os.listdir(cmap_dir)
    for flavor in ("UTF32", "UTF16", "UCS2"):
        for name in sorted(names):
            if name.startswith("Uni") and flavor in name and name.endswith("-H"):
                return name
    raise RuntimeError(f"no Uni*-H CMap in {cmap_dir}")


def cid_to_unicode(cmap_dir: str) -> dict[int, int]:
    """CID -> Unicode code point, inverting the collection's Uni*-H CMap.

    The Uni* CMap maps a Unicode code (UTF-32 or UTF-16, big-endian) to a CID;
    inverting yields CID -> Unicode. UTF-16 surrogate pairs (4-byte codes) are
    decoded to their code point. The smallest Unicode value wins for a CID that
    several codes share.
    """
    name = _find_unicode_cmap(cmap_dir)
    is_utf16 = "UTF32" not in name
    cmap = parse_cmap(cmap_dir, name)

    result: dict[int, int] = {}
    for code, cid in cmap.cid_map().items():
        codepoint = _decode_unicode_code(code, is_utf16)
        if codepoint is None:
            continue
        if cid not in result or codepoint < result[cid]:
            result[cid] = codepoint
    return result


def _decode_unicode_code(code: int, is_utf16: bool) -> int | None:
    if not is_utf16:
        return code
    if code <= 0xFFFF:
        return None if 0xD800 <= code <= 0xDFFF else code
    high, low = code >> 16, code & 0xFFFF
    if 0xD800 <= high <= 0xDBFF and 0xDC00 <= low <= 0xDFFF:
        return 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)
    return None


# --- Range compression ------------------------------------------------------


def compress_unicode(mapping: dict[int, int]) -> list[tuple[int, int, int]]:
    """{cid: unicode} -> [(cid_low, cid_high, unicode_low)] contiguous runs."""
    ranges: list[tuple[int, int, int]] = []
    for cid in sorted(mapping):
        unicode = mapping[cid]
        if ranges:
            low, high, base = ranges[-1]
            if cid == high + 1 and unicode == base + (high - low) + 1:
                ranges[-1] = (low, cid, base)
                continue
        ranges.append((cid, cid, unicode))
    return ranges


def merge_cid_ranges(
    ranges: list[tuple[int, int, int, int]],
) -> list[tuple[int, int, int, int]]:
    """Sort by (width, code_low) and merge adjacent contiguous CID runs."""
    merged: list[tuple[int, int, int, int]] = []
    for low, high, cid, width in sorted(ranges, key=lambda r: (r[3], r[0])):
        if merged:
            p_low, p_high, p_cid, p_width = merged[-1]
            if (
                width == p_width
                and low == p_high + 1
                and cid == p_cid + (p_high - p_low) + 1
            ):
                merged[-1] = (p_low, high, p_cid, p_width)
                continue
        merged.append((low, high, cid, width))
    return merged


# --- Emission ---------------------------------------------------------------


def main() -> None:
    ensure_data()

    collections: list[dict] = []
    cmaps: list[dict] = []

    for index, (directory, registry, ordering) in enumerate(_COLLECTIONS):
        cmap_dir = os.path.join(_DATA_DIR, directory, "CMap")
        unicode_ranges = compress_unicode(cid_to_unicode(cmap_dir))
        collections.append(
            {"registry": registry, "ordering": ordering, "unicode": unicode_ranges}
        )

        for name in sorted(os.listdir(cmap_dir)):
            if name.startswith("Uni") or "." in name:
                continue  # Uni* handled in C++; skip stray non-CMap files
            cmap = parse_cmap(cmap_dir, name)
            if not cmap.cid_ranges:
                continue
            cmaps.append(
                {
                    "name": name,
                    "collection": index,
                    "codespace": sorted(set(cmap.codespace), key=lambda r: r[2]),
                    "cid_ranges": merge_cid_ranges(cmap.cid_ranges),
                }
            )

    _write(collections, cmaps)


def _dedupe(items: list[tuple], pool: list[tuple], offsets: dict) -> tuple[int, int]:
    """Append ``items`` to ``pool`` unless already present; return (offset, count)."""
    key = tuple(items)
    if key in offsets:
        return offsets[key], len(items)
    offset = len(pool)
    offsets[key] = offset
    pool.extend(items)
    return offset, len(items)


def _write(collections: list[dict], cmaps: list[dict]) -> None:
    cmaps.sort(key=lambda c: c["name"])  # binary search at runtime

    codespace_pool: list[tuple] = []
    cid_pool: list[tuple] = []
    unicode_pool: list[tuple] = []
    codespace_offsets: dict = {}
    cid_offsets: dict = {}
    unicode_offsets: dict = {}

    for collection in collections:
        collection["uni_off"], collection["uni_cnt"] = _dedupe(
            collection["unicode"], unicode_pool, unicode_offsets
        )
    for cmap in cmaps:
        cmap["cs_off"], cmap["cs_cnt"] = _dedupe(
            cmap["codespace"], codespace_pool, codespace_offsets
        )
        cmap["cid_off"], cmap["cid_cnt"] = _dedupe(
            cmap["cid_ranges"], cid_pool, cid_offsets
        )

    _write_hpp(len(collections), len(cmaps))
    _write_cpp(collections, cmaps, codespace_pool, cid_pool, unicode_pool)


_BANNER = (
    "// Auto-generated by tools/pdf/generate_cid_data.py -- DO NOT EDIT.\n"
    "// Regenerate with: python3 tools/pdf/generate_cid_data.py\n"
    "// Data: adobe-type-tools/cmap-resources (BSD-3-Clause); see\n"
    "// tools/pdf/THIRD_PARTY_LICENSES.md.\n\n// clang-format off\n\n"
)


def _write_hpp(collection_count: int, cmap_count: int) -> None:
    text = (
        _BANNER
        + f"""#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace odr::internal::pdf::cid_data {{

// A codespace range: codes whose first byte falls in [low, high]'s leading byte
// are `width` bytes wide (big-endian) -- used to split a string into codes.
struct CodespaceRange {{
  std::uint32_t low;
  std::uint32_t high;
  std::uint8_t width;
}};

// A contiguous code -> CID run: code in [code_low, code_high] maps to
// cid_low + (code - code_low). `width` is the code's byte count.
struct CidRange {{
  std::uint32_t code_low;
  std::uint32_t code_high;
  std::uint32_t cid_low;
  std::uint8_t width;
}};

// A contiguous CID -> Unicode run: cid in [cid_low, cid_high] maps to
// unicode_low + (cid - cid_low).
struct UnicodeRange {{
  std::uint32_t cid_low;
  std::uint32_t cid_high;
  char32_t unicode_low;
}};

// A character collection (e.g. Adobe-Japan1): its CID -> Unicode table.
struct Collection {{
  std::string_view registry;
  std::string_view ordering;
  const UnicodeRange *unicode_ranges;
  std::uint32_t unicode_range_count;
}};

// A predefined CMap (code -> CID), pointing at its owning collection.
struct PredefinedCMap {{
  std::string_view name;
  const CodespaceRange *codespace;
  std::uint32_t codespace_count;
  const CidRange *cid_ranges;
  std::uint32_t cid_range_count;
  std::uint32_t collection;
}};

inline constexpr std::size_t collection_count = {collection_count};
inline constexpr std::size_t predefined_cmap_count = {cmap_count};

extern const Collection collections[collection_count];
// Sorted by `name` for binary search.
extern const PredefinedCMap predefined_cmaps[predefined_cmap_count];

}} // namespace odr::internal::pdf::cid_data
"""
    )
    with open(os.path.join(_OUT_DIR, "pdf_cid_data.hpp"), "w", encoding="ascii") as f:
        f.write(text)


def _emit_pool(out: io.StringIO, ctype: str, name: str, pool: list, fmt) -> None:
    out.write(f"const {ctype} {name}[] = {{\n")
    line = "    "
    for item in pool:
        chunk = fmt(item) + ","
        if len(line) + len(chunk) > 96:
            out.write(line + "\n")
            line = "    "
        line += chunk
    if line.strip():
        out.write(line + "\n")
    out.write("};\n\n")


def _write_cpp(
    collections: list[dict],
    cmaps: list[dict],
    codespace_pool: list,
    cid_pool: list,
    unicode_pool: list,
) -> None:
    out = io.StringIO()
    out.write(_BANNER)
    out.write("#include <odr/internal/pdf/pdf_cid_data.hpp>\n\n")
    out.write("namespace odr::internal::pdf::cid_data {\n\nnamespace {\n\n")

    _emit_pool(
        out,
        "CodespaceRange",
        "codespace_pool",
        codespace_pool,
        lambda r: f"{{0x{r[0]:x},0x{r[1]:x},{r[2]}}}",
    )
    _emit_pool(
        out,
        "CidRange",
        "cid_pool",
        cid_pool,
        lambda r: f"{{0x{r[0]:x},0x{r[1]:x},{r[2]},{r[3]}}}",
    )
    _emit_pool(
        out,
        "UnicodeRange",
        "unicode_pool",
        unicode_pool,
        lambda r: f"{{{r[0]},{r[1]},0x{r[2]:x}}}",
    )

    out.write("} // namespace\n\n")

    out.write(f"const Collection collections[collection_count] = {{\n")
    for c in collections:
        out.write(
            f'    {{"{c["registry"]}", "{c["ordering"]}", '
            f"unicode_pool + {c['uni_off']}, {c['uni_cnt']}}},\n"
        )
    out.write("};\n\n")

    out.write(f"const PredefinedCMap predefined_cmaps[predefined_cmap_count] = {{\n")
    for c in cmaps:
        out.write(
            f'    {{"{c["name"]}", codespace_pool + {c["cs_off"]}, {c["cs_cnt"]}, '
            f"cid_pool + {c['cid_off']}, {c['cid_cnt']}, {c['collection']}}},\n"
        )
    out.write("};\n\n")

    out.write("} // namespace odr::internal::pdf::cid_data\n")

    with open(os.path.join(_OUT_DIR, "pdf_cid_data.cpp"), "w", encoding="ascii") as f:
        f.write(out.getvalue())


if __name__ == "__main__":
    main()
