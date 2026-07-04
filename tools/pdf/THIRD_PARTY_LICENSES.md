# Third-party data in `tools/pdf/`

These data files are read by the generator scripts to produce committed C++ in
`src/odr/internal/pdf/`. Some are vendored in this directory; others (the Adobe
Glyph List and the CMap resources) are downloaded from pinned commits and are
git-ignored — see the per-entry notes below.

## Adobe Glyph List — `glyphlist.txt`

- Source: https://github.com/adobe-type-tools/agl-aglfn (`glyphlist.txt`, pinned
  by commit in `_AGL_COMMIT` in `generate_encoding_data.py`)
- Copyright 2002–2019 Adobe (http://www.adobe.com/).
- License: BSD-3-Clause (full text retained in the file's header).

Unlike the base encodings below this file is **not vendored**: the script
downloads it into `tools/pdf/glyphlist.txt` (git-ignored) on first run.
BSD-3-Clause is GPL-compatible, so redistributing this data inside this GPLv3
project is permitted; the obligation to retain the copyright notice and license
text is carried into the generated source's banner.

## Base encodings — `standard_encoding.txt`, `win_ansi_encoding.txt`, `mac_roman_encoding.txt`

The 256-entry StandardEncoding, WinAnsiEncoding and MacRomanEncoding tables
from ISO 32000-1 (PDF 1.7), Annex D. These are factual data from the PDF
specification, transcribed here in the same on-disk format as `glyphlist.txt`.

## Adobe Core-14 AFM metrics — fetched by `generate_afm_data.py`

- Source: the 14 Adobe Core AFM files (`Helvetica`, `Times-*`, `Courier-*`,
  `Symbol`, `ZapfDingbats`), redistributed by Apache PDFBox
  (https://github.com/apache/pdfbox, pinned by tag in `_PDFBOX_TAG` in
  `generate_afm_data.py`).
- The AFM metric files are Adobe's; Apache PDFBox redistributes them under the
  Apache License 2.0.
- License: Apache-2.0 (redistribution), the underlying metric data being
  factual glyph-advance figures from the PDF standard's base-14 fonts.

Like the AGL and CMap resources these files are **not vendored**: the script
downloads them into `tools/pdf/afm/` (git-ignored) on first run. Apache-2.0 is
GPL-compatible; the committed C++ generated from them carries the attribution in
its banner.

## Adobe CMap resources — fetched by `generate_cid_data.py`

- Source: https://github.com/adobe-type-tools/cmap-resources (pinned by commit
  in `generate_cid_data.py`)
- Copyright 1990–2022 Adobe (http://www.adobe.com/).
- License: BSD-3-Clause.

The predefined CMaps for composite (Type0/CID) fonts. Unlike the files above
this archive is **not vendored** in the repository: it is large (~14 MB
unpacked), so the script downloads it into `tools/pdf/cmap-resources/` (which is
git-ignored) on first run. BSD-3-Clause is GPL-compatible; any committed C++
generated from it carries the attribution in its banner.
