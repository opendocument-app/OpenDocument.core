#include "bindings.hpp"

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <pybind11/stl.h>

#include <string>

namespace py = pybind11;

namespace {

// Ties the returned object to `self`: navigation handles keep the originating
// `Document` alive transitively, and so does a `TextStyle`, whose `font_name`
// borrows from the document.
constexpr auto keep_self_alive = py::keep_alive<0, 1>();

py::object make_element_iterator(const odr::ElementRange &range) {
  // The elements need the same tie: pybind11 hands them out by value, which
  // drops the keep-alive `reference_internal` would otherwise imply.
  return py::make_iterator(range.begin(), range.end(), keep_self_alive);
}

py::object make_children_iterator(const odr::Element &element) {
  return make_element_iterator(element.children());
}

/// `__bool__` has to come from the derived type: `Element::operator bool`
/// ignores the typed adapter, so a failed `as_*` cast would look valid.
template <typename T>
py::class_<T, odr::Element> bind_element(py::module_ &m, const char *name) {
  return py::class_<T, odr::Element>(m, name).def("__bool__",
                                                  &T::operator bool);
}

} // namespace

void odr_python::bind_document(py::module_ &m) {
  py::enum_<odr::ElementType>(m, "ElementType")
      .value("none", odr::ElementType::none)
      .value("root", odr::ElementType::root)
      .value("slide", odr::ElementType::slide)
      .value("sheet", odr::ElementType::sheet)
      .value("page", odr::ElementType::page)
      .value("master_page", odr::ElementType::master_page)
      .value("sheet_cell", odr::ElementType::sheet_cell)
      .value("text", odr::ElementType::text)
      .value("line_break", odr::ElementType::line_break)
      .value("page_break", odr::ElementType::page_break)
      .value("paragraph", odr::ElementType::paragraph)
      .value("span", odr::ElementType::span)
      .value("link", odr::ElementType::link)
      .value("bookmark", odr::ElementType::bookmark)
      .value("list", odr::ElementType::list)
      .value("list_item", odr::ElementType::list_item)
      .value("table", odr::ElementType::table)
      .value("table_column", odr::ElementType::table_column)
      .value("table_row", odr::ElementType::table_row)
      .value("table_cell", odr::ElementType::table_cell)
      .value("frame", odr::ElementType::frame)
      .value("image", odr::ElementType::image)
      .value("rect", odr::ElementType::rect)
      .value("line", odr::ElementType::line)
      .value("circle", odr::ElementType::circle)
      .value("custom_shape", odr::ElementType::custom_shape)
      .value("group", odr::ElementType::group);

  py::enum_<odr::AnchorType>(m, "AnchorType")
      .value("as_char", odr::AnchorType::as_char)
      .value("at_char", odr::AnchorType::at_char)
      .value("at_frame", odr::AnchorType::at_frame)
      .value("at_page", odr::AnchorType::at_page)
      .value("at_paragraph", odr::AnchorType::at_paragraph);

  py::enum_<odr::ListType>(m, "ListType")
      .value("unordered", odr::ListType::unordered)
      .value("ordered", odr::ListType::ordered);

  py::enum_<odr::ValueType>(m, "ValueType")
      .value("unknown", odr::ValueType::unknown)
      .value("string", odr::ValueType::string)
      .value("float_number", odr::ValueType::float_number);

  py::class_<odr::TableDimensions>(m, "TableDimensions")
      .def(py::init<>())
      .def(py::init<std::uint32_t, std::uint32_t>(), py::arg("rows"),
           py::arg("columns"))
      .def_readwrite("rows", &odr::TableDimensions::rows)
      .def_readwrite("columns", &odr::TableDimensions::columns)
      .def("__repr__", [](const odr::TableDimensions &dimensions) {
        return "TableDimensions(rows=" + std::to_string(dimensions.rows) +
               ", columns=" + std::to_string(dimensions.columns) + ")";
      });

  py::class_<odr::TablePosition>(m, "TablePosition")
      .def(py::init<>())
      .def(py::init<std::uint32_t, std::uint32_t>(), py::arg("column"),
           py::arg("row"))
      .def_readwrite("column", &odr::TablePosition::column)
      .def_readwrite("row", &odr::TablePosition::row)
      .def_static("to_column_num", &odr::TablePosition::to_column_num,
                  py::arg("string"))
      .def_static("to_row_num", &odr::TablePosition::to_row_num,
                  py::arg("string"))
      .def_static("to_column_string", &odr::TablePosition::to_column_string,
                  py::arg("column"))
      .def_static("to_row_string", &odr::TablePosition::to_row_string,
                  py::arg("row"));

  py::class_<odr::DocumentPath>(m, "DocumentPath")
      .def(py::init<>())
      .def(py::init<const std::string &>(), py::arg("string"))
      .def("empty", &odr::DocumentPath::empty)
      .def("parent", &odr::DocumentPath::parent)
      .def("join", &odr::DocumentPath::join, py::arg("other"))
      .def(
          "__eq__",
          [](const odr::DocumentPath &lhs, const odr::DocumentPath &rhs) {
            return lhs == rhs;
          },
          py::is_operator())
      .def("__str__", &odr::DocumentPath::to_string)
      .def("__repr__", [](const odr::DocumentPath &path) {
        return "DocumentPath('" + path.to_string() + "')";
      });

  py::class_<odr::Element>(m, "Element")
      .def(py::init<>())
      .def("__bool__", &odr::Element::operator bool)
      .def(
          "__eq__",
          [](const odr::Element &lhs, const odr::Element &rhs) {
            return lhs == rhs;
          },
          py::is_operator())
      .def("type", &odr::Element::type)
      .def("parent", &odr::Element::parent, keep_self_alive)
      .def("first_child", &odr::Element::first_child, keep_self_alive)
      .def("previous_sibling", &odr::Element::previous_sibling, keep_self_alive)
      .def("next_sibling", &odr::Element::next_sibling, keep_self_alive)
      .def("is_unique", &odr::Element::is_unique)
      .def("is_self_locatable", &odr::Element::is_self_locatable)
      .def("is_editable", &odr::Element::is_editable)
      .def("document_path", &odr::Element::document_path)
      .def("navigate_path", &odr::Element::navigate_path, py::arg("path"),
           keep_self_alive)
      .def("children", &make_children_iterator, keep_self_alive)
      .def("__iter__", &make_children_iterator, keep_self_alive)
      .def("as_text_root", &odr::Element::as_text_root, keep_self_alive)
      .def("as_slide", &odr::Element::as_slide, keep_self_alive)
      .def("as_sheet", &odr::Element::as_sheet, keep_self_alive)
      .def("as_page", &odr::Element::as_page, keep_self_alive)
      .def("as_sheet_cell", &odr::Element::as_sheet_cell, keep_self_alive)
      .def("as_master_page", &odr::Element::as_master_page, keep_self_alive)
      .def("as_line_break", &odr::Element::as_line_break, keep_self_alive)
      .def("as_paragraph", &odr::Element::as_paragraph, keep_self_alive)
      .def("as_span", &odr::Element::as_span, keep_self_alive)
      .def("as_text", &odr::Element::as_text, keep_self_alive)
      .def("as_link", &odr::Element::as_link, keep_self_alive)
      .def("as_list", &odr::Element::as_list, keep_self_alive)
      .def("as_bookmark", &odr::Element::as_bookmark, keep_self_alive)
      .def("as_list_item", &odr::Element::as_list_item, keep_self_alive)
      .def("as_table", &odr::Element::as_table, keep_self_alive)
      .def("as_table_column", &odr::Element::as_table_column, keep_self_alive)
      .def("as_table_row", &odr::Element::as_table_row, keep_self_alive)
      .def("as_table_cell", &odr::Element::as_table_cell, keep_self_alive)
      .def("as_frame", &odr::Element::as_frame, keep_self_alive)
      .def("as_rect", &odr::Element::as_rect, keep_self_alive)
      .def("as_line", &odr::Element::as_line, keep_self_alive)
      .def("as_circle", &odr::Element::as_circle, keep_self_alive)
      .def("as_custom_shape", &odr::Element::as_custom_shape, keep_self_alive)
      .def("as_image", &odr::Element::as_image, keep_self_alive);

  bind_element<odr::TextRoot>(m, "TextRoot")
      .def("page_layout", &odr::TextRoot::page_layout)
      .def("first_master_page", &odr::TextRoot::first_master_page,
           keep_self_alive);

  bind_element<odr::Slide>(m, "Slide")
      .def("name", &odr::Slide::name)
      .def("page_layout", &odr::Slide::page_layout)
      .def("master_page", &odr::Slide::master_page, keep_self_alive);

  bind_element<odr::Sheet>(m, "Sheet")
      .def("name", &odr::Sheet::name)
      .def("dimensions", &odr::Sheet::dimensions)
      .def("content", &odr::Sheet::content, py::arg("range"))
      .def("cell", &odr::Sheet::cell, py::arg("column"), py::arg("row"),
           keep_self_alive)
      .def(
          "shapes",
          [](const odr::Sheet &sheet) {
            return make_element_iterator(sheet.shapes());
          },
          keep_self_alive)
      .def("style", &odr::Sheet::style)
      .def("column_style", &odr::Sheet::column_style, py::arg("column"))
      .def("row_style", &odr::Sheet::row_style, py::arg("row"))
      .def("cell_style", &odr::Sheet::cell_style, py::arg("column"),
           py::arg("row"));

  bind_element<odr::SheetCell>(m, "SheetCell")
      .def("position", &odr::SheetCell::position)
      .def("is_covered", &odr::SheetCell::is_covered)
      .def("span", &odr::SheetCell::span)
      .def("value_type", &odr::SheetCell::value_type);

  bind_element<odr::Page>(m, "Page")
      .def("name", &odr::Page::name)
      .def("page_layout", &odr::Page::page_layout)
      .def("master_page", &odr::Page::master_page, keep_self_alive);

  bind_element<odr::MasterPage>(m, "MasterPage")
      .def("page_layout", &odr::MasterPage::page_layout);

  bind_element<odr::LineBreak>(m, "LineBreak")
      .def("style", &odr::LineBreak::style, keep_self_alive);

  bind_element<odr::Paragraph>(m, "Paragraph")
      .def("style", &odr::Paragraph::style)
      .def("text_style", &odr::Paragraph::text_style, keep_self_alive);

  bind_element<odr::Span>(m, "Span").def("style", &odr::Span::style,
                                         keep_self_alive);

  bind_element<odr::Text>(m, "Text")
      .def("content", &odr::Text::content)
      .def("set_content", &odr::Text::set_content, py::arg("text"))
      .def("style", &odr::Text::style, keep_self_alive);

  bind_element<odr::Link>(m, "Link").def("href", &odr::Link::href);

  bind_element<odr::Bookmark>(m, "Bookmark").def("name", &odr::Bookmark::name);

  bind_element<odr::List>(m, "List").def("list_type", &odr::List::type);

  bind_element<odr::ListItem>(m, "ListItem")
      .def("style", &odr::ListItem::style, keep_self_alive)
      .def("marker", &odr::ListItem::marker)
      .def("number", &odr::ListItem::number);

  bind_element<odr::Table>(m, "Table")
      .def("first_row", &odr::Table::first_row, keep_self_alive)
      .def("first_column", &odr::Table::first_column, keep_self_alive)
      .def(
          "columns",
          [](const odr::Table &table) {
            return make_element_iterator(table.columns());
          },
          keep_self_alive)
      .def(
          "rows",
          [](const odr::Table &table) {
            return make_element_iterator(table.rows());
          },
          keep_self_alive)
      .def("dimensions", &odr::Table::dimensions)
      .def("style", &odr::Table::style);

  bind_element<odr::TableColumn>(m, "TableColumn")
      .def("style", &odr::TableColumn::style);

  bind_element<odr::TableRow>(m, "TableRow")
      .def("style", &odr::TableRow::style);

  bind_element<odr::TableCell>(m, "TableCell")
      .def("is_covered", &odr::TableCell::is_covered)
      .def("span", &odr::TableCell::span)
      .def("value_type", &odr::TableCell::value_type)
      .def("style", &odr::TableCell::style);

  bind_element<odr::Frame>(m, "Frame")
      .def("anchor_type", &odr::Frame::anchor_type)
      .def("x", &odr::Frame::x)
      .def("y", &odr::Frame::y)
      .def("width", &odr::Frame::width)
      .def("height", &odr::Frame::height)
      .def("z_index", &odr::Frame::z_index)
      .def("style", &odr::Frame::style);

  bind_element<odr::Rect>(m, "Rect")
      .def("x", &odr::Rect::x)
      .def("y", &odr::Rect::y)
      .def("width", &odr::Rect::width)
      .def("height", &odr::Rect::height)
      .def("style", &odr::Rect::style);

  bind_element<odr::Line>(m, "Line")
      .def("x1", &odr::Line::x1)
      .def("y1", &odr::Line::y1)
      .def("x2", &odr::Line::x2)
      .def("y2", &odr::Line::y2)
      .def("style", &odr::Line::style);

  bind_element<odr::Circle>(m, "Circle")
      .def("x", &odr::Circle::x)
      .def("y", &odr::Circle::y)
      .def("width", &odr::Circle::width)
      .def("height", &odr::Circle::height)
      .def("style", &odr::Circle::style);

  bind_element<odr::CustomShape>(m, "CustomShape")
      .def("x", &odr::CustomShape::x)
      .def("y", &odr::CustomShape::y)
      .def("width", &odr::CustomShape::width)
      .def("height", &odr::CustomShape::height)
      .def("style", &odr::CustomShape::style);

  bind_element<odr::Image>(m, "Image")
      .def("is_internal", &odr::Image::is_internal)
      .def("file", &odr::Image::file)
      .def("href", &odr::Image::href);

  py::class_<odr::Document>(m, "Document")
      .def("is_editable", &odr::Document::is_editable)
      .def("is_savable", &odr::Document::is_savable,
           py::arg("encrypted") = false)
      // saving serialises the whole document; holding the GIL for it blocks
      // every other Python thread
      .def("save",
           py::overload_cast<const std::string &>(&odr::Document::save,
                                                  py::const_),
           py::arg("path"), py::call_guard<py::gil_scoped_release>())
      .def("save",
           py::overload_cast<const std::string &, const std::string &>(
               &odr::Document::save, py::const_),
           py::arg("path"), py::arg("password"),
           py::call_guard<py::gil_scoped_release>())
      .def("file_type", &odr::Document::file_type)
      .def("document_type", &odr::Document::document_type)
      .def("root_element", &odr::Document::root_element, keep_self_alive)
      .def("as_filesystem", &odr::Document::as_filesystem);
}
