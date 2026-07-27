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

## Detected but not decoded

These are recognised by `list_file_types` / `mimetype` so a caller can report
them, but there is no decoder — opening one throws:

- rtf
- wpd (WordPerfect)

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

Notable changes are in [`CHANGELOG.md`](CHANGELOG.md), which also states what
counts as public API and what stability to expect. Release history is on
[GitHub](https://github.com/opendocument-app/OpenDocument.core/releases).

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
