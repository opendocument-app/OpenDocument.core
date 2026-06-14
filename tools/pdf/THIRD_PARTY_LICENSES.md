# Third-party data in `tools/pdf/`

These data files are vendored from third parties and read by
`generate_encoding_data.py` to produce `src/odr/internal/pdf/pdf_encoding_data.{hpp,cpp}`.

## Adobe Glyph List — `glyphlist.txt`

- Source: https://github.com/adobe-type-tools/agl-aglfn (`glyphlist.txt`)
- Copyright 2002–2019 Adobe (http://www.adobe.com/).
- License: BSD-3-Clause (full text retained in the file's header).

BSD-3-Clause is GPL-compatible, so redistributing this data inside this
GPLv3 project is permitted; the only obligation — retaining the copyright
notice and license text — is met by keeping the header intact in `glyphlist.txt`.

## Base encodings — `standard_encoding.txt`, `win_ansi_encoding.txt`, `mac_roman_encoding.txt`

The 256-entry StandardEncoding, WinAnsiEncoding and MacRomanEncoding tables
from ISO 32000-1 (PDF 1.7), Annex D. These are factual data from the PDF
specification, transcribed here in the same on-disk format as `glyphlist.txt`.
