# `tools/pdf`

Developer tooling for the PDF engine (`src/odr/internal/pdf/`). Not part of the
build — these scripts generate committed C++ source from vendored data.

## `generate_encoding_data.py`

Generates `src/odr/internal/pdf/pdf_encoding_data.{hpp,cpp}` — the three
base-encoding tables and the Adobe Glyph List used to map simple-font
`/Encoding` glyph names to Unicode. The build only compiles the generated
result, so there is no build-time dependency on Python.

Regenerate (no arguments; all source data is vendored next to the script):

```bash
python3 tools/pdf/generate_encoding_data.py
```

### Vendored data files

All four share one on-disk convention: `#` comment lines, blank lines ignored,
and two `;`-delimited fields per record.

| File | Contents | Format |
|------|----------|--------|
| `glyphlist.txt` | Adobe Glyph List | `glyphname;CCCC CCCC` (hex UTF-16 code points) |
| `standard_encoding.txt` | StandardEncoding (ISO 32000-1 Annex D) | `HH;glyphname` (sparse; missing = .notdef) |
| `win_ansi_encoding.txt` | WinAnsiEncoding | `HH;glyphname` |
| `mac_roman_encoding.txt` | MacRomanEncoding | `HH;glyphname` |

To refresh the AGL, replace `glyphlist.txt` from
[adobe-type-tools/agl-aglfn](https://github.com/adobe-type-tools/agl-aglfn) and
rerun the script. The base-encoding tables are fixed by the PDF spec and should
not change.

See [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the provenance and
licensing of the vendored data.
