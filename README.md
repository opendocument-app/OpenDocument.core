# OpenDocument.core

![build status](https://github.com/opendocument-app/OpenDocument.core/workflows/build_test/badge.svg)

C++ library to visualize files, especially documents, in HTML.

## Supported files

- [odt](https://github.com/opendocument-app/OpenDocument.core/issues/92), [odp](https://github.com/opendocument-app/OpenDocument.core/issues/93), [ods](https://github.com/opendocument-app/OpenDocument.core/issues/94), [odg](https://github.com/opendocument-app/OpenDocument.core/issues/96) ([OpenOffice / LibreOffice](https://github.com/opendocument-app/OpenDocument.core/issues/111))
- [docx](https://github.com/opendocument-app/OpenDocument.core/issues/86), [pptx](https://github.com/opendocument-app/OpenDocument.core/issues/85), [xlsx](https://github.com/opendocument-app/OpenDocument.core/issues/87) ([Microsoft Office Open XML](https://github.com/opendocument-app/OpenDocument.core/issues/112))
- [csv](https://github.com/opendocument-app/OpenDocument.core/issues/107)
- [doc](https://github.com/opendocument-app/OpenDocument.core/issues/104), [ppt](https://github.com/opendocument-app/OpenDocument.core/issues/106), [xls](https://github.com/opendocument-app/OpenDocument.core/issues/105)
- [pdf](https://github.com/opendocument-app/OpenDocument.core/issues/108)
- txt
- json
- [zip](https://github.com/opendocument-app/OpenDocument.core/issues/109)
- [cfb](https://github.com/opendocument-app/OpenDocument.core/issues/110) (Microsoft Compound File Binary File Format)
- ttf / otf (font specimen pages)

Every accepted file extension and MIME type — including the template and
macro-enabled aliases (`docm`, `dotx`, `xltx`, `ppsx`, …) — is enumerable at
runtime via `file_extensions_by_file_type` / `mimetypes_by_file_type`, so a
consumer never has to maintain its own copy of these tables.

## Detected but not decoded

These are classified by extension and MIME type, and `rtf`/`wpd` are also
recognised from their bytes, so a caller can report them — but there is no
decoder, and opening one throws:

- rtf
- wpd (WordPerfect)
- md (Markdown)
- xlsb (Excel binary workbook — an OOXML package whose workbook parts are
  binary rather than spreadsheetml)

## Asking what is supported

`capabilities_by_file_type(FileType)` answers, per format, whether the library
can detect, open, decrypt, render, edit, save or encrypt it — the decisions a
caller has to make *before* it holds a file, e.g. which MIME types to advertise
to a system file picker. It is a declared upper bound; `DecodedFile::capabilities()`
and `Document::is_editable` / `is_savable` give the precise answer for a
concrete file.

Editing and saving are currently limited to odt, odp, odg (`edit` + `save`),
ods (`save` only) and docx (`edit` + `save`); saving with a password is not
supported for any format.

## Unsupported files

- pages
- xml
- yaml

## References

Currently, used as backend for [OpenDocument.droid](https://github.com/opendocument-app/OpenDocument.droid) and [OpenDocument.ios](https://github.com/opendocument-app/OpenDocument.ios).

Replaces legacy projects [OpenDocument.java](https://github.com/andiwand/OpenDocument.java), [JOpenDocument](https://github.com/andiwand/JOpenDocument) and [svm](https://github.com/andiwand/svm).

Potential test files: https://file-examples.com/

## [Documentation](docs/README.md)

## Tooling

- [`tools/pdf`](tools/pdf/README.md) — generators for the PDF engine's committed encoding data.

## Build

This project comes with CMake as a build system and Conan as package manager. In principle they should be independent and one can build without Conan.

Using Conan one can use our Artifactory as a Conan remote for convenience: https://artifactory.opendocument.app/

As an alternative to the Conan remote you can also export the package locally via Conan i.e. `conan export . --name odrcore --version VERSION` (fill `VERSION` with something appropriate).

## Version

Versions and history are tracked on [GitHub](https://github.com/opendocument-app/OpenDocument.core).

## Testing

### Running the HTML Comparison Server

Scripts and Docker images can be found here https://github.com/opendocument-app/compare-html

```bash
./test/scripts/compare_output_server.sh
```

## License

OpenDocument.core is licensed under the [Mozilla Public License 2.0](LICENSE)
(`MPL-2.0`).

The committed PDF font/encoding tables in `src/odr/internal/pdf/` are
generated from third-party source data whose provenance and terms are documented
in [`tools/pdf/THIRD_PARTY_LICENSES.md`](tools/pdf/THIRD_PARTY_LICENSES.md).
