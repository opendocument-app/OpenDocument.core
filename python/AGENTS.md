# AGENTS.md — python bindings

pybind11 bindings for the public C++ API (`src/odr/*.hpp`), packaged as
`pyodr`.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | Builds the `pyodr/_core` extension (target `pyodr_core`). Included from the root build via `ODR_PYTHON`, or standalone against an installed `odrcore`. |
| `src/` | One `bind_*` unit per public-API area. `bindings.hpp` declares them. |
| `pyodr/` | Pure-python package. `__init__.py` re-exports `_core`; `cli.py` is the `pyodr` console script. |
| `tests/` | pytest suite. `conftest.py` builds every input inline (tmp files, a zip-built minimal ODT). No fixture files. |

## Rules

- Bind the public API only. Never include `odr/internal/...` headers.
- Mirror the C++ names. A `Logger` parameter is an optional trailing `logger`
  argument that defaults to `Logger.null()`.
- `ILogger` is bound with the trampoline `PyLogger` in `bind_logger.cpp`, so a
  Python class can implement a sink. Two rules there matter only at teardown,
  so a passing happy-path test proves nothing:
  - `adopt_sink` hands the sink to C++ as a `shared_ptr` that owns a
    reference to the Python object. Otherwise `Logger(MySink())` leaves C++
    with a dead object.
  - `flush()` dispatches through `py::get_override`, not
    `PYBIND11_OVERRIDE_PURE`. Sinks flush from destructors, and a "pure
    virtual not implemented" exception there aborts the process.
- `LogLevel` is bound with `py::arithmetic()`, so `level >= LogLevel.warning`
  works inside `will_log`.
- Anything that returns an `Element` (or a subtype or iterator) carries
  `py::keep_alive<0, 1>()` (`keep_self_alive` in `bind_document.cpp`), so a
  handle keeps its `Document` alive.
- Stream-based C++ APIs (`write`, `save`, `pipe`) return `bytes` or `str`
  through `std::ostringstream`.
- New public C++ API: extend the matching `bind_*.cpp` and add a pytest.
- C++ follows the repo clang-format. Python is formatted with `black`, and CI
  checks it.
- Tests stay hermetic. Build inputs inline in `tests/conftest.py`. Tests that
  need the HTTP server skip on `pyodr.has_http_server`.
- Build and test loop: [`README.md`](README.md). CI: `.github/workflows/python.yml`.
