# `tools/pdf`

Generators for the PDF engine (`src/odr/internal/pdf/`). They are not part of
the build. Each writes committed C++ source from third-party data, so the build
has no dependency on Python. See
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the provenance and
the terms of the data.

Each script downloads its input from a pinned commit or tag into a git-ignored
cache next to the script on first run, and reuses it afterwards. To refresh the
data, bump the pin in the script, delete the cache, and rerun.

| Script | Output | Input | Cache |
|---|---|---|---|
| `generate_encoding_data.py` | `pdf_encoding_data.{hpp,cpp}`: StandardEncoding, WinAnsiEncoding, MacRomanEncoding and the Adobe Glyph List, for simple-font `/Encoding` glyph names | `{standard,win_ansi,mac_roman}_encoding.txt` (vendored, fixed by ISO 32000-1 Annex D) and the AGL from [agl-aglfn](https://github.com/adobe-type-tools/agl-aglfn) (`_AGL_COMMIT`) | `glyphlist.txt` |
| `generate_cid_data.py` | `pdf_cid_data.{hpp,cpp}`: `code → CID` per predefined legacy CMap and `CID → Unicode` per character collection, for composite fonts (RKSJ, EUC, Big5, GBK, KSC) | [cmap-resources](https://github.com/adobe-type-tools/cmap-resources) (`_COMMIT`) | `cmap-resources/` |
| `generate_afm_data.py` | `pdf_afm_data.{hpp,cpp}`: glyph widths of the base-14 fonts, for a non-embedded font with no `/Widths` | the Core-14 AFM files as Apache PDFBox ships them (`_PDFBOX_TAG`) | `afm/` |

```bash
python3 tools/pdf/generate_encoding_data.py
python3 tools/pdf/generate_cid_data.py
python3 tools/pdf/generate_afm_data.py
```

The vendored `*_encoding.txt` files share one format: `#` comment lines, blank
lines ignored, two `;`-delimited fields per record (`HH;glyphname`, sparse,
missing means `.notdef`). The AGL uses `glyphname;CCCC CCCC` with hex UTF-16
code points.

The Unicode CMaps (`Uni*-UCS2/UTF16/UTF32`) need no table. `pdf_cid.cpp`
handles them directly, because their codes are Unicode already.
