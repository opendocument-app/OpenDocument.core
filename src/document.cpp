#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr {

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_document{std::move(document)} {
  if (!m_document) {
    throw std::runtime_error("document is null");
  }
}

bool Document::editable() const noexcept { return m_document->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_document->savable(encrypted);
}

void Document::save(const std::string &path) const { m_document->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_document->save(path, password.c_str());
}

DocumentType Document::document_type() const noexcept {
  return m_document->document_type();
}

DocumentCursor Document::root_element() const {
  return {m_document, m_document->root_element()};
}

DocumentCursor::DocumentCursor(
    std::shared_ptr<internal::abstract::Document> document,
    std::shared_ptr<internal::abstract::DocumentCursor> cursor)
    : m_document{std::move(document)}, m_cursor{std::move(cursor)} {
  // TODO throw if nullptr
}

bool DocumentCursor::operator==(const DocumentCursor &rhs) const {
  return m_cursor->equals(*rhs.m_cursor);
}

bool DocumentCursor::operator!=(const DocumentCursor &rhs) const {
  return !operator==(rhs);
}

std::string DocumentCursor::document_path() const {
  return m_cursor->document_path();
}

ElementType DocumentCursor::element_type() const {
  return m_cursor->element()->type(m_document.get());
}

bool DocumentCursor::move_to_parent() { return m_cursor->move_to_parent(); }

bool DocumentCursor::move_to_first_child() {
  return m_cursor->move_to_first_child();
}

bool DocumentCursor::move_to_previous_sibling() {
  return m_cursor->move_to_previous_sibling();
}

bool DocumentCursor::move_to_next_sibling() {
  return m_cursor->move_to_next_sibling();
}

Element DocumentCursor::element() const {
  return {m_document.get(), m_cursor->element()};
}

bool DocumentCursor::move_to_master_page() {
  return m_cursor->move_to_master_page();
}

bool DocumentCursor::move_to_first_table_column() {
  return m_cursor->move_to_first_table_column();
}

bool DocumentCursor::move_to_first_table_row() {
  return m_cursor->move_to_first_table_row();
}

void DocumentCursor::for_each_child(const ChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_column(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_column()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_row(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_row()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_cell(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_(const ChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    visitor(*this, i);
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

void DocumentCursor::for_each_(const ConditionalChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    if (!visitor(*this, i)) {
      break;
    }
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

Element::Element(const internal::abstract::Document *document,
                 internal::abstract::Element *element)
    : m_document{document}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element->equals(m_document, *rhs.m_element);
}

bool Element::operator!=(const Element &rhs) const { return !operator==(rhs); }

Element::operator bool() const { return m_element; }

ElementType Element::type() const { return m_element->type(m_document); }

std::string Element::Text::value() const {
  return m_element ? m_element->value(m_document) : "";
}

std::string Element::Link::href() const {
  return m_element ? m_element->href(m_document) : "";
}

std::string Element::Bookmark::name() const {
  return m_element ? m_element->name(m_document) : "";
}

TableDimensions Element::Table::dimensions() const {
  return m_element ? m_element->dimensions(m_document) : TableDimensions();
}

TableDimensions Element::TableCell::span() const {
  return m_element ? m_element->span(m_document) : TableDimensions();
}

std::optional<std::string> Element::Frame::anchor_type() const {
  return m_element ? m_element->anchor_type(m_document)
                   : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::x() const {
  return m_element ? m_element->x(m_document) : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::y() const {
  return m_element ? m_element->y(m_document) : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::width() const {
  return m_element ? m_element->width(m_document)
                   : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::height() const {
  return m_element ? m_element->height(m_document)
                   : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::z_index() const {
  return m_element ? m_element->z_index(m_document)
                   : std::optional<std::string>();
}

std::string Element::Rect::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::string Element::Rect::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string Element::Rect::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string Element::Rect::height() const {
  return m_element ? m_element->height(m_document) : "";
}

std::string Element::Line::x1() const {
  return m_element ? m_element->x1(m_document) : "";
}

std::string Element::Line::y1() const {
  return m_element ? m_element->y1(m_document) : "";
}

std::string Element::Line::x2() const {
  return m_element ? m_element->x2(m_document) : "";
}

std::string Element::Line::y2() const {
  return m_element ? m_element->y2(m_document) : "";
}

std::string Element::Circle::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::string Element::Circle::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string Element::Circle::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string Element::Circle::height() const {
  return m_element ? m_element->height(m_document) : "";
}

std::optional<std::string> Element::CustomShape::x() const {
  return m_element ? m_element->x(m_document) : "";
}

std::optional<std::string> Element::CustomShape::y() const {
  return m_element ? m_element->y(m_document) : "";
}

std::string Element::CustomShape::width() const {
  return m_element ? m_element->width(m_document) : "";
}

std::string Element::CustomShape::height() const {
  return m_element ? m_element->height(m_document) : "";
}

bool Element::Image::internal() const {
  return m_element ? m_element->internal(m_document) : false;
}

std::optional<File> Element::Image::file() const {
  return m_element ? m_element->file(m_document) : std::optional<File>();
}

std::string Element::Image::href() const {
  return m_element ? m_element->href(m_document) : "";
}

Element::Text Element::text() const { return {m_document, m_element}; }

Element::Link Element::link() const { return {m_document, m_element}; }

Element::Bookmark Element::bookmark() const { return {m_document, m_element}; }

Element::Table Element::table() const { return {m_document, m_element}; }

Element::TableColumn Element::table_column() const {
  return {m_document, m_element};
}

Element::TableRow Element::table_row() const { return {m_document, m_element}; }

Element::TableCell Element::table_cell() const {
  return {m_document, m_element};
}

Element::Frame Element::frame() const { return {m_document, m_element}; }

Element::Rect Element::rect() const { return {m_document, m_element}; }

Element::Line Element::line() const { return {m_document, m_element}; }

Element::Circle Element::circle() const { return {m_document, m_element}; }

Element::CustomShape Element::custom_shape() const {
  return {m_document, m_element};
}

Element::Image Element::image() const { return {m_document, m_element}; }

Property PageLayout::width() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->width(m_document) : nullptr, m_style_context};
}

Property TextStyle::font_name() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_name(m_document) : nullptr, m_style_context};
}

Property TextStyle::font_size() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_size(m_document) : nullptr, m_style_context};
}

Property TextStyle::font_weight() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_weight(m_document) : nullptr,
          m_style_context};
}

Property TextStyle::font_style() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_style(m_document) : nullptr, m_style_context};
}

Property TextStyle::font_underline() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_underline(m_document) : nullptr,
          m_style_context};
}

Property TextStyle::font_line_through() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_line_through(m_document) : nullptr,
          m_style_context};
}

Property TextStyle::font_shadow() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_shadow(m_document) : nullptr,
          m_style_context};
}

Property TextStyle::font_color() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->font_color(m_document) : nullptr, m_style_context};
}

Property TextStyle::background_color() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->background_color(m_document) : nullptr,
          m_style_context};
}

Property ParagraphStyle::text_align() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->text_align(m_document) : nullptr, m_style_context};
}

DirectionalProperty ParagraphStyle::margin() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->margin(m_document) : nullptr, m_style_context};
}

Property TableStyle::width() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->width(m_document) : nullptr, m_style_context};
}

Property TableColumnStyle::width() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->width(m_document) : nullptr, m_style_context};
}

Property TableRowStyle::height() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->height(m_document) : nullptr, m_style_context};
}

Property TableCellStyle::vertical_align() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->vertical_align(m_document) : nullptr,
          m_style_context};
}

Property TableCellStyle::background_color() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->background_color(m_document) : nullptr,
          m_style_context};
}

DirectionalProperty TableCellStyle::padding() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->padding(m_document) : nullptr, m_style_context};
}

DirectionalProperty TableCellStyle::border() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->border(m_document) : nullptr, m_style_context};
}

Property GraphicStyle::stroke_width() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->stroke_width(m_document) : nullptr,
          m_style_context};
}

Property GraphicStyle::stroke_color() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->stroke_color(m_document) : nullptr,
          m_style_context};
}

Property GraphicStyle::fill_color() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->fill_color(m_document) : nullptr, m_style_context};
}

Property GraphicStyle::vertical_align() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->vertical_align(m_document) : nullptr,
          m_style_context};
}

Property PageLayout::height() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->height(m_document) : nullptr, m_style_context};
}

Property PageLayout::print_orientation() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->print_orientation(m_document) : nullptr,
          m_style_context};
}

DirectionalProperty PageLayout::margin() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->margin(m_document) : nullptr, m_style_context};
}

Property::Property(const internal::abstract::Document *document,
                   const internal::abstract::Element *element,
                   const internal::abstract::Style *style,
                   const internal::abstract::Property *property,
                   const StyleContext style_context)
    : m_document{document}, m_element{element}, m_style{style},
      m_property{property}, m_style_context{style_context} {}

Property::operator bool() const { return m_property; }

std::optional<std::string> Property::value() const {
  if (!*this) {
    return {};
  }
  return m_property->value(m_document, m_element, m_style, m_style_context);
}

DirectionalProperty::DirectionalProperty(
    const internal::abstract::Document *document,
    const internal::abstract::Element *element,
    const internal::abstract::Style *style,
    internal::abstract::DirectionalProperty *property,
    const StyleContext style_context)
    : m_document{document}, m_element{element}, m_style{style},
      m_property{property}, m_style_context{style_context} {}

DirectionalProperty::operator bool() const { return m_property; }

Property DirectionalProperty::right() const {
  return {m_document, m_element, m_style,
          m_property ? m_property->right(m_document) : nullptr,
          m_style_context};
}

Property DirectionalProperty::top() const {
  return {m_document, m_element, m_style,
          m_property ? m_property->top(m_document) : nullptr, m_style_context};
}

Property DirectionalProperty::left() const {
  return {m_document, m_element, m_style,
          m_property ? m_property->left(m_document) : nullptr, m_style_context};
}

Property DirectionalProperty::bottom() const {
  return {m_document, m_element, m_style,
          m_property ? m_property->bottom(m_document) : nullptr,
          m_style_context};
}

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

} // namespace odr
