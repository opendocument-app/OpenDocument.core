# AGENTS.md — python bindings

pybind11 bindings for the public C++ API (`src/odr/*.hpp`), packaged as
`pyodr`.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | Builds `pyodr/_core` extension; included from the root build via `ODR_PYTHON`, or standalone against an installed `odrcore`. |
| `src/` | Binding sources, one `bind_*` unit per public-API area (`bindings.hpp` declares them). |
| `pyodr/` | Pure-python package: `__init__.py` re-exports `_core`, `cli.py` is the `pyodr` console script. |
| `tests/` | pytest suite; inputs are generated inline (tmp files, zip-built minimal ODT) — no fixture files. |

## Rules

- **Bind the public API only** — never include `odr/internal/...` headers.
- Mirror the C++ names; drop the `Logger` parameters (bindings use the default
  null logger).
- Anything returning an `Element` (or subtype/iterator) must carry
  `py::keep_alive<0, 1>()` so handles keep the originating `Document` alive
  (see `keep_self_alive` in `bind_document.cpp`).
- Stream-based C++ APIs (`write`, `save`, `pipe`) are bound as functions
  returning `bytes`/`str` via `std::ostringstream`.
- New public C++ API? Extend the matching `bind_*.cpp` and add a pytest.
- C++ sources follow the repo clang-format; python is formatted with `black`.
- Tests must stay hermetic: build inputs inline in `tests/conftest.py`;
  HTML-rendering tests take the `core_data_path` fixture (skips when assets are
  missing).
- Build/test loop: see `python/README.md`; CI lives in
  `.github/workflows/python.yml`.
