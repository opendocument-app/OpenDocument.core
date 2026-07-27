#pragma once

#include <pybind11/pybind11.h>

namespace odr_python {

void bind_core(pybind11::module_ &m);
void bind_logger(pybind11::module_ &m);
void bind_style(pybind11::module_ &m);
void bind_file(pybind11::module_ &m);
void bind_document(pybind11::module_ &m);
void bind_html(pybind11::module_ &m);
void bind_http_server(pybind11::module_ &m);
void bind_functions(pybind11::module_ &m);

} // namespace odr_python
