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


def split_runs(
    ranges: list[tuple[int, int, int, int]],
) -> list[tuple[int, int, int, int]]:
    """(code_low, code_high, cid_low, width) -> (code_low, cid_low, run_len, width).

    ``run_len = code_high - code_low`` is capped at 255 (a byte) by splitting an
    over-long run into consecutive chunks; 36 of ~86k ranges need it. The split
    happens before interning, so the chunks of identical over-long runs still
    de-duplicate.
    """
    out: list[tuple[int, int, int, int]] = []
    for code_low, code_high, cid_low, width in ranges:
        total = code_high - code_low
        offset = 0
        while offset <= total:
            run = min(255, total - offset)
            out.append((code_low + offset, cid_low + offset, run, width))
            offset += run + 1
    return out


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


def build_collection(cmap_dir: str) -> dict:
    """Pack a collection's CID -> Unicode map into the S3 representation.

    A presence bitmap over ``[0, max_cid]`` (one bit per CID) plus a rank index
    (cumulative popcount before each 64-bit word) turns ``CID -> Unicode`` into
    ``values[rank(cid)]``. Values are ``uint16``; the ~1% astral (> U+FFFF) ones
    store the ``0xffff`` sentinel and their real code point in ``astral``
    (sorted by value index for binary search).
    """
    c2u = cid_to_unicode(cmap_dir)
    max_cid = max(c2u)
    word_count = max_cid // 64 + 1
    bitmap = [0] * word_count
    values: list[int] = []
    astral: list[tuple[int, int]] = []
    for cid in sorted(c2u):
        bitmap[cid // 64] |= 1 << (cid % 64)
        codepoint = c2u[cid]
        if codepoint > 0xFFFF:
            astral.append((len(values), codepoint))
            values.append(0xFFFF)
        else:
            values.append(codepoint)
    rank = []
    accumulated = 0
    for word in bitmap:
        rank.append(accumulated)
        accumulated += word.bit_count()
    return {
        "max_cid": max_cid,
        "bitmap": bitmap,
        "rank": rank,
        "values": values,
        "astral": astral,
    }


def main() -> None:
    ensure_data()

    collections: list[dict] = []
    cmaps: list[dict] = []

    for index, (directory, registry, ordering) in enumerate(_COLLECTIONS):
        cmap_dir = os.path.join(_DATA_DIR, directory, "CMap")
        collection = build_collection(cmap_dir)
        collection["registry"] = registry
        collection["ordering"] = ordering
        collections.append(collection)

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
                    # (code_low, cid_low, run_len, width), run_len <= 255.
                    "ranges": split_runs(merge_cid_ranges(cmap.cid_ranges)),
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


def _intern_ranges(cmaps: list[dict]) -> tuple[list[tuple], list[int]]:
    """Intern each CMap's code -> CID ranges into a shared pool.

    ~73% of the ~86k ranges repeat across CMaps (``-H``/``-V`` pairs, shared
    encoding families), so each CMap stores only ``uint16`` indices into a pool
    of unique records. Each CMap's index slice is sorted by ``(width, code_low)``
    for the runtime binary search; identical slices (fully shared range sets) are
    de-duplicated too.
    """
    pool: list[tuple] = []
    pool_index: dict[tuple, int] = {}
    refs: list[int] = []
    ref_offsets: dict = {}
    for cmap in cmaps:
        indices = []
        for record in cmap["ranges"]:
            index = pool_index.get(record)
            if index is None:
                index = len(pool)
                pool_index[record] = index
                pool.append(record)
            indices.append(index)
        indices.sort(key=lambda i: (pool[i][3], pool[i][0]))  # (width, code_low)
        cmap["ref_off"], cmap["ref_cnt"] = _dedupe(indices, refs, ref_offsets)
    return pool, refs


def _write(collections: list[dict], cmaps: list[dict]) -> None:
    cmaps.sort(key=lambda c: c["name"])  # binary search at runtime

    cid_range_pool, ref_pool = _intern_ranges(cmaps)

    codespace_pool: list[tuple] = []
    codespace_offsets: dict = {}
    for cmap in cmaps:
        cmap["cs_off"], cmap["cs_cnt"] = _dedupe(
            cmap["codespace"], codespace_pool, codespace_offsets
        )

    # Concatenate the per-collection unicode pools, tracking each one's offset.
    bitmap_pool: list[int] = []
    rank_pool: list[int] = []
    values_pool: list[int] = []
    astral_pool: list[tuple] = []
    for collection in collections:
        collection["bitmap_off"] = len(bitmap_pool)
        bitmap_pool.extend(collection["bitmap"])
        rank_pool.extend(collection["rank"])
        collection["values_off"] = len(values_pool)
        values_pool.extend(collection["values"])
        collection["astral_off"] = len(astral_pool)
        astral_pool.extend(collection["astral"])

    _write_hpp(len(collections), len(cmaps), len(cid_range_pool))
    _write_cpp(
        collections,
        cmaps,
        cid_range_pool,
        ref_pool,
        codespace_pool,
        bitmap_pool,
        rank_pool,
        values_pool,
        astral_pool,
    )


_BANNER = (
    "// Auto-generated by tools/pdf/generate_cid_data.py -- DO NOT EDIT.\n"
    "// Regenerate with: python3 tools/pdf/generate_cid_data.py\n"
    "// Data: adobe-type-tools/cmap-resources (BSD-3-Clause); see\n"
    "// tools/pdf/THIRD_PARTY_LICENSES.md.\n\n// clang-format off\n\n"
)


def _write_hpp(collection_count: int, cmap_count: int, pool_size: int) -> None:
    text = (
        _BANNER
        + f"""#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace odr::internal::pdf::cid_data {{

// A codespace range: a code whose first byte lands in the leading byte of
// [low, high] is `width` bytes wide (big-endian) -- used to split a string into
// codes. Matched on the first byte, as the runtime CMap does (9.7.6.2).
struct CodespaceRange {{
  std::uint32_t low;
  std::uint32_t high;
  std::uint8_t width;
}};

// A contiguous code -> CID run: code in [code_low, code_low + run_len] maps to
// cid_low + (code - code_low). `width` is the code's byte count. Runs are split
// so run_len fits a byte, and records are interned (many CMaps share them) --
// each PredefinedCMap indexes this pool through `cid_range_pool`.
struct CidRange {{
  std::uint32_t code_low;
  std::uint16_t cid_low;
  std::uint8_t run_len;
  std::uint8_t width;
}};

// One astral (> U+FFFF) CID -> Unicode value: the dense-value slot carrying the
// 0xffff sentinel, plus its real code point. Sorted by `value_index`.
struct AstralEntry {{
  std::uint32_t value_index;
  char32_t codepoint;
}};

// A character collection (e.g. Adobe-Japan1): its dense CID -> Unicode map as a
// presence bitmap over [0, max_cid] (LSB-first within each 64-bit word), a rank
// index (popcount of all words before word i), and a value array indexed by
// rank(cid). A value of 0xffff escapes to `astral`.
struct Collection {{
  std::string_view registry;
  std::string_view ordering;
  const std::uint64_t *bitmap;
  const std::uint32_t *rank;
  std::uint32_t word_count;
  std::uint32_t max_cid;
  const std::uint16_t *values;
  const AstralEntry *astral;
  std::uint32_t astral_count;
}};

// A predefined CMap (code -> CID). `ranges` indexes `cid_range_pool`, sorted by
// (width, code_low) for binary search; `collection` indexes `collections`.
struct PredefinedCMap {{
  std::string_view name;
  const CodespaceRange *codespace;
  std::uint32_t codespace_count;
  const std::uint16_t *ranges;
  std::uint32_t range_count;
  std::uint32_t collection;
}};

inline constexpr std::size_t collection_count = {collection_count};
inline constexpr std::size_t predefined_cmap_count = {cmap_count};
inline constexpr std::size_t cid_range_pool_size = {pool_size};

extern const std::array<CidRange, cid_range_pool_size> cid_range_pool;
extern const std::array<Collection, collection_count> collections;
// Sorted by `name` for binary search.
extern const std::array<PredefinedCMap, predefined_cmap_count> predefined_cmaps;

}} // namespace odr::internal::pdf::cid_data
"""
    )
    with open(os.path.join(_OUT_DIR, "pdf_cid_data.hpp"), "w", encoding="ascii") as f:
        f.write(text)


def _emit_array(
    out: io.StringIO,
    ctype: str,
    name: str,
    pool: list,
    fmt,
    std_array: bool = False,
    size_expr: str | None = None,
) -> None:
    """Emit a ``const std::array<T, N> name = {{...}};`` definition.

    ``size_expr`` names the extent (e.g. the header's ``cid_data::foo_size``
    constant); it defaults to the literal element count. ``name``/``ctype`` may be
    namespace-qualified so the definition binds to the header declaration.
    """
    extent = size_expr if size_expr is not None else str(len(pool))
    if std_array:
        # std::array needs the double brace (aggregate wrapping a C array).
        out.write(f"const std::array<{ctype}, {extent}>\n    {name} = {{{{\n")
    else:
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
    out.write("}};\n\n" if std_array else "};\n\n")


def _write_cpp(
    collections: list[dict],
    cmaps: list[dict],
    cid_range_pool: list,
    ref_pool: list,
    codespace_pool: list,
    bitmap_pool: list,
    rank_pool: list,
    values_pool: list,
    astral_pool: list,
) -> None:
    out = io.StringIO()
    out.write(_BANNER)
    out.write("#include <odr/internal/pdf/pdf_cid_data.hpp>\n\n")

    # The intern/lookup pools are implementation detail: anonymous (internal
    # linkage) inside the data namespace. Every one is a std::array; the exported
    # structs hold raw pointers into them (`pool.data() + offset`), since a
    # per-CMap / per-collection span has no compile-time size of its own.
    out.write("namespace odr::internal::pdf::cid_data {\n\nnamespace {\n\n")
    _emit_array(
        out,
        "CodespaceRange",
        "codespace_pool",
        codespace_pool,
        lambda r: f"{{0x{r[0]:x},0x{r[1]:x},{r[2]}}}",
        std_array=True,
    )
    _emit_array(out, "std::uint16_t", "range_ref_pool", ref_pool, str, std_array=True)
    _emit_array(
        out, "std::uint64_t", "bitmap_pool", bitmap_pool, lambda w: f"0x{w:x}",
        std_array=True,
    )
    _emit_array(out, "std::uint32_t", "rank_pool", rank_pool, str, std_array=True)
    _emit_array(
        out, "std::uint16_t", "values_pool", values_pool, lambda v: f"0x{v:x}",
        std_array=True,
    )
    _emit_array(
        out,
        "AstralEntry",
        "astral_pool",
        astral_pool,
        lambda a: f"{{{a[0]},0x{a[1]:x}}}",
        std_array=True,
    )
    out.write("} // namespace\n\n} // namespace odr::internal::pdf::cid_data\n\n")

    # The exported (header-declared) arrays are defined in the parent namespace
    # and qualified with `cid_data::`, so a rename in the header fails to compile
    # here rather than silently defining a new symbol.
    out.write("namespace odr::internal::pdf {\n\n")

    _emit_array(
        out,
        "cid_data::CidRange",
        "cid_data::cid_range_pool",
        cid_range_pool,
        lambda r: f"{{0x{r[0]:x},{r[1]},{r[2]},{r[3]}}}",
        std_array=True,
        size_expr="cid_data::cid_range_pool_size",
    )

    out.write(
        "const std::array<cid_data::Collection, cid_data::collection_count>\n"
        "    cid_data::collections = {{\n"
    )
    for c in collections:
        out.write(
            f'    {{"{c["registry"]}", "{c["ordering"]}", '
            f"cid_data::bitmap_pool.data() + {c['bitmap_off']}, "
            f"cid_data::rank_pool.data() + {c['bitmap_off']}, "
            f"{len(c['bitmap'])}, {c['max_cid']}, "
            f"cid_data::values_pool.data() + {c['values_off']}, "
            f"cid_data::astral_pool.data() + {c['astral_off']}, "
            f"{len(c['astral'])}}},\n"
        )
    out.write("}};\n\n")

    out.write(
        "const std::array<cid_data::PredefinedCMap, "
        "cid_data::predefined_cmap_count>\n    cid_data::predefined_cmaps = {{\n"
    )
    for c in cmaps:
        out.write(
            f'    {{"{c["name"]}", cid_data::codespace_pool.data() + '
            f"{c['cs_off']}, {c['cs_cnt']}, cid_data::range_ref_pool.data() + "
            f"{c['ref_off']}, {c['ref_cnt']}, {c['collection']}}},\n"
        )
    out.write("}};\n\n")

    out.write("} // namespace odr::internal::pdf\n")

    with open(os.path.join(_OUT_DIR, "pdf_cid_data.cpp"), "w", encoding="ascii") as f:
        f.write(out.getvalue())


if __name__ == "__main__":
    main()
