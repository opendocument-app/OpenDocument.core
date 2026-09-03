#include "bindings.hpp"

#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <pybind11/stl.h>

#include <optional>
#include <string>

namespace py = pybind11;

namespace {

template <typename T>
void bind_directional_style(py::module_ &m, const char *name) {
  py::class_<odr::DirectionalStyle<T>>(m, name)
      .def(py::init<>())
      .def_readwrite("right", &odr::DirectionalStyle<T>::right)
      .def_readwrite("top", &odr::DirectionalStyle<T>::top)
      .def_readwrite("left", &odr::DirectionalStyle<T>::left)
      .def_readwrite("bottom", &odr::DirectionalStyle<T>::bottom);
}

} // namespace

void odr_python::bind_style(py::module_ &m) {
  py::enum_<odr::FontWeight>(m, "FontWeight")
      .value("normal", odr::FontWeight::normal)
      .value("bold", odr::FontWeight::bold);

  py::enum_<odr::FontStyle>(m, "FontStyle")
      .value("normal", odr::FontStyle::normal)
      .value("italic", odr::FontStyle::italic);

  py::enum_<odr::FontPosition>(m, "FontPosition")
      .value("normal", odr::FontPosition::normal)
      .value("super", odr::FontPosition::super)
      .value("sub", odr::FontPosition::sub);

  py::enum_<odr::BreakType>(m, "BreakType")
      .value("none", odr::BreakType::none)
      .value("page", odr::BreakType::page)
      .value("column", odr::BreakType::column);

  py::enum_<odr::TextAlign>(m, "TextAlign")
      .value("left", odr::TextAlign::left)
      .value("right", odr::TextAlign::right)
      .value("center", odr::TextAlign::center)
      .value("justify", odr::TextAlign::justify);

  py::enum_<odr::HorizontalAlign>(m, "HorizontalAlign")
      .value("left", odr::HorizontalAlign::left)
      .value("center", odr::HorizontalAlign::center)
      .value("right", odr::HorizontalAlign::right);

  py::enum_<odr::VerticalAlign>(m, "VerticalAlign")
      .value("top", odr::VerticalAlign::top)
      .value("middle", odr::VerticalAlign::middle)
      .value("bottom", odr::VerticalAlign::bottom);

  py::enum_<odr::PrintOrientation>(m, "PrintOrientation")
      .value("portrait", odr::PrintOrientation::portrait)
      .value("landscape", odr::PrintOrientation::landscape);

  py::enum_<odr::TextWrap>(m, "TextWrap")
      .value("none", odr::TextWrap::none)
      .value("before", odr::TextWrap::before)
      .value("after", odr::TextWrap::after)
      .value("run_through", odr::TextWrap::run_through);

  py::class_<odr::DynamicUnit>(m, "DynamicUnit")
      .def(py::init<>())
      .def(py::init([](const std::string &name) {
             return odr::DynamicUnit(name.c_str());
           }),
           py::arg("name"))
      .def("name", &odr::DynamicUnit::name)
      .def(
          "__eq__",
          [](const odr::DynamicUnit &lhs, const odr::DynamicUnit &rhs) {
            return lhs == rhs;
          },
          py::is_operator())
      .def("__str__", &odr::DynamicUnit::to_string)
      .def("__repr__", [](const odr::DynamicUnit &unit) {
        return "DynamicUnit('" + unit.to_string() + "')";
      });

  py::class_<odr::Measure>(m, "Measure")
      .def(py::init([](const std::string &string) {
             return odr::Measure(string.c_str());
           }),
           py::arg("string"))
      .def(py::init<double, odr::DynamicUnit>(), py::arg("magnitude"),
           py::arg("unit"))
      .def("magnitude", &odr::Measure::magnitude)
      .def("unit", &odr::Measure::unit)
      .def("__float__", [](const odr::Measure &q) { return q.magnitude(); })
      .def(
          "__eq__",
          [](const odr::Measure &lhs, const odr::Measure &rhs) {
            return lhs == rhs;
          },
          py::is_operator())
      .def("__str__", &odr::Measure::to_string)
      .def("__repr__", [](const odr::Measure &q) {
        return "Measure('" + q.to_string() + "')";
      });

  py::class_<odr::Color>(m, "Color")
      .def(py::init<>())
      .def_static("from_rgb", &odr::Color::from_rgb, py::arg("rgb"))
      .def_static("from_argb", &odr::Color::from_argb, py::arg("argb"))
      .def(py::init<std::uint8_t, std::uint8_t, std::uint8_t>(), py::arg("red"),
           py::arg("green"), py::arg("blue"))
      .def(py::init<std::uint8_t, std::uint8_t, std::uint8_t, std::uint8_t>(),
           py::arg("red"), py::arg("green"), py::arg("blue"), py::arg("alpha"))
      .def_readwrite("red", &odr::Color::red)
      .def_readwrite("green", &odr::Color::green)
      .def_readwrite("blue", &odr::Color::blue)
      .def_readwrite("alpha", &odr::Color::alpha)
      .def("rgb", &odr::Color::rgb)
      .def("argb", &odr::Color::argb);

  bind_directional_style<odr::Measure>(m, "DirectionalMeasure");
  bind_directional_style<std::string>(m, "DirectionalString");

  py::class_<odr::TextStyle>(m, "TextStyle")
      .def(py::init<>())
      .def_property_readonly(
          "font_name",
          [](const odr::TextStyle &style) -> std::optional<std::string> {
            if (!style.font_name.has_value()) {
              return std::nullopt;
            }
            return std::string(*style.font_name);
          })
      .def_readwrite("font_size", &odr::TextStyle::font_size)
      .def_readwrite("font_weight", &odr::TextStyle::font_weight)
      .def_readwrite("font_style", &odr::TextStyle::font_style)
      .def_readwrite("font_underline", &odr::TextStyle::font_underline)
      .def_readwrite("font_line_through", &odr::TextStyle::font_line_through)
      .def_readwrite("font_shadow", &odr::TextStyle::font_shadow)
      .def_readwrite("font_color", &odr::TextStyle::font_color)
      .def_readwrite("background_color", &odr::TextStyle::background_color)
      .def_readwrite("font_position", &odr::TextStyle::font_position);

  py::class_<odr::ParagraphStyle>(m, "ParagraphStyle")
      .def(py::init<>())
      .def_readwrite("text_align", &odr::ParagraphStyle::text_align)
      .def_readwrite("margin", &odr::ParagraphStyle::margin)
      .def_readwrite("line_height", &odr::ParagraphStyle::line_height)
      .def_readwrite("text_indent", &odr::ParagraphStyle::text_indent)
      .def_readwrite("break_before", &odr::ParagraphStyle::break_before)
      .def_readwrite("break_after", &odr::ParagraphStyle::break_after);

  py::class_<odr::TableStyle>(m, "TableStyle")
      .def(py::init<>())
      .def_readwrite("width", &odr::TableStyle::width)
      .def_readwrite("border", &odr::TableStyle::border)
      .def_readwrite("border_inside_horizontal",
                     &odr::TableStyle::border_inside_horizontal)
      .def_readwrite("border_inside_vertical",
                     &odr::TableStyle::border_inside_vertical);

  py::class_<odr::TableColumnStyle>(m, "TableColumnStyle")
      .def(py::init<>())
      .def_readwrite("width", &odr::TableColumnStyle::width);

  py::class_<odr::TableRowStyle>(m, "TableRowStyle")
      .def(py::init<>())
      .def_readwrite("height", &odr::TableRowStyle::height);

  py::class_<odr::TableCellStyle>(m, "TableCellStyle")
      .def(py::init<>())
      .def_readwrite("horizontal_align", &odr::TableCellStyle::horizontal_align)
      .def_readwrite("vertical_align", &odr::TableCellStyle::vertical_align)
      .def_readwrite("background_color", &odr::TableCellStyle::background_color)
      .def_readwrite("padding", &odr::TableCellStyle::padding)
      .def_readwrite("border", &odr::TableCellStyle::border)
      .def_readwrite("text_rotation", &odr::TableCellStyle::text_rotation);

  py::class_<odr::GraphicStyle>(m, "GraphicStyle")
      .def(py::init<>())
      .def_readwrite("stroke_width", &odr::GraphicStyle::stroke_width)
      .def_readwrite("stroke_color", &odr::GraphicStyle::stroke_color)
      .def_readwrite("fill_color", &odr::GraphicStyle::fill_color)
      .def_readwrite("vertical_align", &odr::GraphicStyle::vertical_align)
      .def_readwrite("text_wrap", &odr::GraphicStyle::text_wrap)
      .def_readwrite("horizontal_position",
                     &odr::GraphicStyle::horizontal_position);

  py::class_<odr::PageLayout>(m, "PageLayout")
      .def(py::init<>())
      .def_readwrite("width", &odr::PageLayout::width)
      .def_readwrite("height", &odr::PageLayout::height)
      .def_readwrite("print_orientation", &odr::PageLayout::print_orientation)
      .def_readwrite("margin", &odr::PageLayout::margin)
      .def_readwrite("background_color", &odr::PageLayout::background_color);
}
