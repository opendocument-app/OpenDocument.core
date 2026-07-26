# pyodr — Python bindings for OpenDocument.core

Decode office documents (ODF, OOXML, legacy MS binary, PDF, CSV, ...) and
render them to HTML from Python.

```python
import pyodr

file = pyodr.open("document.odt")
print(pyodr.file_type_to_string(file.file_type()))

service = pyodr.html.translate(file, "cache-dir", pyodr.HtmlConfig())
html = service.bring_offline("output-dir")
for page in html.pages():
    print(page.name, page.path)
```

A small CLI is included: `pyodr <file>` renders a document and opens it in the
browser; `pyodr <file> --serve` hosts it over HTTP (via the core HTTP server,
available when built with `ODR_WITH_HTTP_SERVER`).

## Building

The bindings are part of the main CMake build, toggled by `ODR_PYTHON`:

```bash
conan install . -o '&:with_python=True' --build missing
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DODR_PYTHON=ON
cmake --build build --target pyodr_core
PYTHONPATH=build/python ODR_CORE_DATA_PATH=build/data python -m pytest python/tests
```

`pip install .` from the repository root builds a wheel via scikit-build-core
(see the root `pyproject.toml`); run `conan install` first and point
`CMAKE_ARGS` at the generated `conan_toolchain.cmake` so the C++ dependencies
resolve. Wheels build without pdf2htmlEX/wvWare (their runtime data cannot ship
inside the wheel) but with libmagic, whose database is bundled — so match those
options:

```bash
conan install . -o '&:with_python=True' -o '&:with_pdf2htmlEX=False' \
    -o '&:with_wvWare=False' -o '&:bundle_assets=True' --build missing
CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$PWD/conan_toolchain.cmake" pip install .
```

## Runtime data

Rendering uses shipped assets (CSS/JS), and MIME detection uses libmagic's
compiled database (`magic.mgc`). Wheels bundle both under `pyodr/data` and pick
them up automatically. For in-tree builds set `ODR_CORE_DATA_PATH` (the tests
read it) and, if needed, `ODR_LIBMAGIC_DATABASE_PATH`, or call
`pyodr.GlobalParams.set_odr_core_data_path(...)` /
`set_libmagic_database_path(...)`.

Neither is fatal when missing: without the assets, rendering fails on the
individual resource; without the database, odrcore tries the system database and
then falls back to its own magic sniffing (which sees an `.odt` as
`application/zip` rather than the ODF type).
