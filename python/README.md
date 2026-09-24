# pyodr — Python bindings for OpenDocument.core

Decode office documents (ODF, OOXML, legacy MS binary, PDF, CSV, ...) and
render them to HTML from Python.

```python
import pyodr

file = pyodr.open("document.odt")
print(pyodr.file_type_to_string(file.file_type()))

service = pyodr.html.translate(file, pyodr.HtmlConfig())
html = service.bring_offline("output-dir")
for page in html.pages():
    print(page.name, page.path)
```

The `pyodr <file>` command renders a document and opens it in the browser.
`pyodr <file> --serve` hosts it over HTTP. The server needs a build with
`ODR_WITH_HTTP_SERVER`.

## Building

The bindings are part of the main CMake build, toggled by `ODR_PYTHON`:

```bash
conan install . -o '&:with_python=True' --build missing
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DODR_PYTHON=ON
cmake --build build --target pyodr_core
PYTHONPATH=build/python python -m pytest python/tests
```

`pip install .` from the repository root builds a wheel with scikit-build-core
(root `pyproject.toml`). Run `conan install` first and point `CMAKE_ARGS` at
the generated toolchain file:

```bash
conan install . -o '&:with_python=True' --build missing
CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$PWD/conan_toolchain.cmake" pip install .
```

## Runtime data

There is none. The renderer's css and js are part of the library, and MIME
detection needs no database. `mimetype` runs the open strategy, so it names
what is inside a zip or a compound file.
