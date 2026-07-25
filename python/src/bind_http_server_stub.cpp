#include "bindings.hpp"

namespace py = pybind11;

// Compiled instead of `bind_http_server.cpp` when `ODR_WITH_HTTP_SERVER` is
// OFF; see `python/CMakeLists.txt`.
void odr_python::bind_http_server(py::module_ &m) {
  m.attr("has_http_server") = false;
}
