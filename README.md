# OpenDocument.core

[![Build](https://github.com/opendocument-app/OpenDocument.core/actions/workflows/build_test.yml/badge.svg)](https://github.com/opendocument-app/OpenDocument.core/actions/workflows/build_test.yml)
[![Release](https://img.shields.io/github/v/release/opendocument-app/OpenDocument.core)](https://github.com/opendocument-app/OpenDocument.core/releases)
[![PyPI](https://img.shields.io/pypi/v/pyodr.svg)](https://pypi.org/project/pyodr/)
[![npm](https://img.shields.io/npm/v/@opendocument/odr-core.svg)](https://www.npmjs.com/package/@opendocument/odr-core)
[![Maven Central](https://img.shields.io/maven-central/v/app.opendocument/odr-core-android.svg)](https://central.sonatype.com/artifact/app.opendocument/odr-core-android)
[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-brightgreen.svg)](LICENSE)

A C++ library that reads documents and renders them to HTML, and edits and
saves the office formats. It is the engine behind
[OpenDocument Reader](https://opendocument.app/) on
[Android](https://github.com/opendocument-app/OpenDocument.droid) and
[iOS](https://github.com/opendocument-app/OpenDocument.ios).

```cpp
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <iostream>

const odr::DecodedFile file = odr::open("document.odt");
const odr::HtmlService service = odr::html::translate(file, odr::HtmlConfig());
const odr::Html html = service.bring_offline("output-dir");
for (const odr::HtmlPage &page : html.pages()) {
  std::cout << page.name << ": " << page.path << '\n';
}
```

An editable page records the user's edits as an op log, and
`Document::edit` replays it onto the file before a save. See the
[editing design](docs/design/editing.md).

## Formats

| Format | Read | Edit and save | Notes |
|---|---|---|---|
| odt, odp, ods, odg, and the flat xml forms fodt, fodp, fods, fodg | yes | yes | OpenDocument. Encrypted files are decrypted. |
| docx, pptx, xlsx | yes | yes | Office Open XML. Encrypted files are decrypted. |
| doc, ppt, xls | yes | no | Legacy Microsoft binary. Text and direct formatting; ppt also supports JPEG/PNG pictures. Encrypted files are decrypted. |
| pdf | yes | no | Own parser. Encrypted files are decrypted. Supports annotation. |
| pages, key, numbers | yes | no | Apple iWork. Text and tables. No styles, images, masters, presenter notes, number formats or merged cells. Password-protected files are decrypted. |
| rtf | yes | no | Body text only. |
| md | yes | no | CommonMark plus GitHub extensions. No raw html, images or horizontal rules. A `.md` or `.markdown` name selects it for text input; unnamed bytes need `FileType::markdown`. |
| txt | yes | yes | |
| csv | yes | yes | Cell values only. Saves as UTF-8. |
| json, xml | yes | no | |
| zip, cfb | yes | no | Archive listing. |
| png, gif, jpeg, bmp, webp, tiff, heif, avif, jxl, ico, svg, svm | yes | no | Images. |
| ttf, otf | yes | no | Font specimen page. |
| mp3, m4a, ogg, wav, flac, mp4, mov, 3gp, mkv, avi | passthrough | no | Detected by content; the browser plays the original bytes in an html media page. |

Detected but not decoded: wpd, xlsb and html. A caller can name them, but
opening one throws. Saving with a password is not supported for any format.

`capabilities_by_file_type(FileType)` says per format whether the library can
detect, open, decrypt, render, edit, save or encrypt it. It is a declared upper
bound. `DecodedFile::capabilities()` and `Document::is_editable` answer for a
concrete file. `file_extensions_by_file_type` and `mimetypes_by_file_type` list
every accepted extension and MIME type, including aliases such as `docm`,
`dotx`, `xltx` and `ppsx`.

## Bindings

| Platform | Package | Docs |
|---|---|---|
| Python | [`pyodr`](https://pypi.org/project/pyodr/) | [python](python/README.md) |
| Java, Android | [`app.opendocument:odr-core-android`](https://central.sonatype.com/artifact/app.opendocument/odr-core-android) | [jni](jni/README.md), [android](android/README.md) |
| Swift (Apple) | `OdrCore`, a Swift package | [apple](apple/README.md) |
| JavaScript (WebAssembly) | [`@opendocument/odr-core`](https://www.npmjs.com/package/@opendocument/odr-core) | [wasm](wasm/README.md) |
| C++ | `odrcore` on [Conan](https://artifactory.opendocument.app/) | [Build](#build) |

## Build

The build requires CMake 3.20 or newer, Conan 2 and a C++23 compiler. Public
headers support C++20 consumers. See [`AGENTS.md`](AGENTS.md) for the build and
test loop and the coding conventions.

A Conan remote is at https://artifactory.opendocument.app/. You can also export
the package locally:

```bash
conan export . --name odrcore --version VERSION
```

## Tests

After a test run, check that every embedded font survives
[OTS](https://github.com/khaledhosny/ots). A rejected font renders as tofu.

```bash
pip install opentype-sanitizer
python test/scripts/check_fonts.py build/test/output
```

Check that the text layer of each pdf page holds the words that
[Ghostscript](https://www.ghostscript.com) reads from the source pdf. A file
fails below its floor in `test/scripts/pdf_text_floors.json`.

```bash
brew install ghostscript
python test/scripts/check_pdf_text.py test/data/input build/test/output
```

`SaveCorpus` edits a few words in every editable corpus document, saves it,
and keeps the file under `build/test/output/saved`. Our own reader accepts
packages that LibreOffice refuses, so load the saved files in LibreOffice
before a change to a save path lands.

```bash
python test/scripts/check_saved_files.py build/test/output/saved
```

To compare the rendered output against the reference output in a browser, run
the [compare-html](https://github.com/opendocument-app/compare-html) server:

```bash
./test/scripts/compare_output_server.sh
```

## Documentation

- [`AGENTS.md`](AGENTS.md): architecture, directory map, conventions, releasing
- [`docs/design/`](docs/design/README.md): design decisions
- [`tools/pdf`](tools/pdf/README.md): generators for the PDF encoding data
- [`CHANGELOG.md`](CHANGELOG.md) and the [releases](https://github.com/opendocument-app/OpenDocument.core/releases)

## License

[Mozilla Public License 2.0](LICENSE). The PDF font and encoding tables in
`src/odr/internal/pdf/` come from third-party data. See
[`tools/pdf/THIRD_PARTY_LICENSES.md`](tools/pdf/THIRD_PARTY_LICENSES.md).
