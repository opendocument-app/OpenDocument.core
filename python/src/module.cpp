#include "bindings.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_core, m) {
  m.doc() = "Python bindings for OpenDocument.core";

  odr_python::bind_core(m);
  odr_python::bind_logger(m);
  odr_python::bind_style(m);
  odr_python::bind_file(m);
  odr_python::bind_document(m);
  odr_python::bind_html(m);
  odr_python::bind_http_server(m);
  odr_python::bind_functions(m);
}
