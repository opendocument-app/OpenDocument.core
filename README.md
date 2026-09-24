# OpenDocument.core

![build status](https://github.com/opendocument-app/OpenDocument.core/actions/workflows/build_test.yml/badge.svg)

A C++ library that reads documents and renders them to HTML. It is the engine
behind [OpenDocument.droid](https://github.com/opendocument-app/OpenDocument.droid)
and [OpenDocument.ios](https://github.com/opendocument-app/OpenDocument.ios).

## Formats

| Format | Read | Edit and save | Notes |
|---|---|---|---|
| odt, odp, ods, odg, and the flat xml forms fodt, fodp, fods, fodg | yes | yes | OpenDocument. Encrypted files are decrypted. |
| docx, pptx, xlsx | yes | yes | Office Open XML. Encrypted files are decrypted. |
| doc, ppt, xls | yes | no | Legacy Microsoft binary. Visible text only. |
| pdf | yes | no | Own parser. Encrypted files are decrypted. Supports annotation. |
| pages, key, numbers | yes | no | Apple iWork. Text and tables. No styles, images, masters, presenter notes, number formats or merged cells. |
| rtf | yes | no | Body text only. |
| md | yes | no | CommonMark plus GitHub extensions. No raw html, images or horizontal rules. Detected by name only, so open it as `FileType::markdown`. |
| txt | yes | yes | |
| csv, json, xml | yes | no | |
| zip, cfb | yes | no | Archive listing. |
| png, gif, jpeg, bmp, webp, tiff, heif, avif, jxl, ico, svg, svm | yes | no | Images. |
| ttf, otf | yes | no | Font specimen page. |
| mp3, m4a, ogg, wav, flac, mp4, mov, 3gp, mkv, avi | passthrough | no | The bytes go to the browser as they are, in an html media page. Not detected by content. |

Detected but not decoded: wpd, xlsb and html. A caller can name them, but
opening one throws. Saving with a password is not supported for any format.

`capabilities_by_file_type(FileType)` says per format whether the library can
detect, open, decrypt, render, edit, save or encrypt it. It is a declared upper
bound. `DecodedFile::capabilities()` and `Document::is_editable` answer for a
concrete file. `file_extensions_by_file_type` and `mimetypes_by_file_type` list
every accepted extension and MIME type, including aliases such as `docm`,
`dotx`, `xltx` and `ppsx`.

## Bindings

- [Python](python/README.md): `pyodr`
- [Java/JNI](jni/README.md) and [Android](android/README.md): `app.opendocument:odr-core-android`
- [Apple](apple/README.md): `OdrCore`, a Swift package
- [WebAssembly](wasm/README.md): `@opendocument/odr-core`

## Build

The build uses CMake and Conan. See [`AGENTS.md`](AGENTS.md) for the build and
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
