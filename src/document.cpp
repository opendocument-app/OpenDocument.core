#include <cstdint>
#include <internal/abstract/document.h>
#include <internal/common/document_path.h>
#include <internal/common/path.h>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/style.h>
#include <stdexcept>
#include <utility>

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
    std::unique_ptr<internal::abstract::DocumentCursor> cursor)
    : m_document{std::move(document)}, m_cursor{std::move(cursor)} {
  // TODO throw if nullptr
}

DocumentCursor::DocumentCursor(const DocumentCursor &other)
    : DocumentCursor(other.m_document, other.m_cursor->copy()) {}

DocumentCursor::~DocumentCursor() = default;

DocumentCursor &DocumentCursor::operator=(const DocumentCursor &other) {
  if (&other == this) {
    return *this;
  }

  m_document = other.m_document;
  m_cursor = other.m_cursor->copy();
  return *this;
}

bool DocumentCursor::operator==(const DocumentCursor &rhs) const {
  return m_cursor->equals(*rhs.m_cursor);
}

bool DocumentCursor::operator!=(const DocumentCursor &rhs) const {
  return !operator==(rhs);
}

std::string DocumentCursor::document_path() const {
  return m_cursor->document_path().to_string();
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
  return {m_document.get(), m_cursor.get(), m_cursor->element()};
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

bool DocumentCursor::move_to_first_sheet_shape() {
  return m_cursor->move_to_first_sheet_shape();
}

void DocumentCursor::move(const std::string &path) {
  m_cursor->move(internal::common::DocumentPath(path));
}

void DocumentCursor::for_each_child(const ChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_column(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_column()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_row(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_row()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_cell(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_sheet_shape(const ChildVisitor &visitor) {
  if (!move_to_first_sheet_shape()) {
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
                 const internal::abstract::DocumentCursor *cursor,
                 internal::abstract::Element *element)
    : m_document{document}, m_cursor{cursor}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element->equals(m_document, m_cursor, *rhs.m_element);
}

bool Element::operator!=(const Element &rhs) const { return !operator==(rhs); }

Element::operator bool() const { return m_element; }

ElementType Element::type() const { return m_element->type(m_document); }

PageLayout Element::TextRoot::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::string Element::Slide::name() const {
  return m_element ? m_element->name(m_document) : "";
}

PageLayout Element::Slide::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::string Element::Sheet::name() const {
  return m_element ? m_element->name(m_document) : "";
}

TableDimensions
Element::Sheet::content(std::optional<TableDimensions> range) const {
  return m_element ? m_element->content(m_document, range) : TableDimensions();
}

PageLayout Element::Page::page_layout() const {
  return m_element ? m_element->page_layout(m_document) : PageLayout();
}

std::optional<TextStyle> Element::LineBreak::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::optional<ParagraphStyle> Element::Paragraph::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<ParagraphStyle>();
}

std::optional<TextStyle> Element::Paragraph::text_style() const {
  return m_element ? m_element->text_style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::optional<TextStyle> Element::Span::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::string Element::Text::content() const {
  return m_element ? m_element->content(m_document) : "";
}

void Element::Text::set_content(const std::string &text) const {
  if (m_element) {
    m_element->set_content(m_document, text);
  }
}

std::optional<TextStyle> Element::Text::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

std::string Element::Link::href() const {
  return m_element ? m_element->href(m_document) : "";
}

std::string Element::Bookmark::name() const {
  return m_element ? m_element->name(m_document) : "";
}

std::optional<TextStyle> Element::ListItem::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TextStyle>();
}

TableDimensions Element::Table::dimensions() const {
  return m_element ? m_element->dimensions(m_document) : TableDimensions();
}

std::optional<TableStyle> Element::Table::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableStyle>();
}

std::optional<TableColumnStyle> Element::TableColumn::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableColumnStyle>();
}

std::optional<TableRowStyle> Element::TableRow::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableRowStyle>();
}

Element::TableColumn Element::TableCell::column() const {
  return m_element ? TableColumn(m_document, m_cursor,
                                 m_element->column(m_document, m_cursor))
                   : TableColumn();
}

Element::TableRow Element::TableCell::row() const {
  return m_element ? TableRow(m_document, m_cursor,
                              m_element->row(m_document, m_cursor))
                   : TableRow();
}

bool Element::TableCell::covered() const {
  return m_element && m_element->covered(m_document);
}

TableDimensions Element::TableCell::span() const {
  return m_element ? m_element->span(m_document) : TableDimensions();
}

ValueType Element::TableCell::value_type() const {
  return m_element ? m_element->value_type(m_document) : ValueType::string;
}

std::optional<TableCellStyle> Element::TableCell::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<TableCellStyle>();
}

AnchorType Element::Frame::anchor_type() const {
  return m_element ? m_element->anchor_type(m_document)
                   : AnchorType::as_char; // TODO default?
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

std::optional<GraphicStyle> Element::Frame::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
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

std::optional<GraphicStyle> Element::Rect::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
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

std::optional<GraphicStyle> Element::Line::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
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

std::optional<GraphicStyle> Element::Circle::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
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

std::optional<GraphicStyle> Element::CustomShape::style() const {
  return m_element ? m_element->style(m_document, m_cursor)
                   : std::optional<GraphicStyle>();
}

bool Element::Image::internal() const {
  return m_element && m_element->internal(m_document);
}

std::optional<File> Element::Image::file() const {
  return m_element ? m_element->file(m_document) : std::optional<File>();
}

std::string Element::Image::href() const {
  return m_element ? m_element->href(m_document) : "";
}

Element::TextRoot Element::text_root() const {
  return {m_document, m_cursor, m_element};
}

Element::Slide Element::slide() const {
  return {m_document, m_cursor, m_element};
}

Element::Sheet Element::sheet() const {
  return {m_document, m_cursor, m_element};
}

Element::Page Element::page() const {
  return {m_document, m_cursor, m_element};
}

Element::LineBreak Element::line_break() const {
  return {m_document, m_cursor, m_element};
}

Element::Paragraph Element::paragraph() const {
  return {m_document, m_cursor, m_element};
}

Element::Span Element::span() const {
  return {m_document, m_cursor, m_element};
}

Element::Text Element::text() const {
  return {m_document, m_cursor, m_element};
}

Element::Link Element::link() const {
  return {m_document, m_cursor, m_element};
}

Element::Bookmark Element::bookmark() const {
  return {m_document, m_cursor, m_element};
}

Element::ListItem Element::list_item() const {
  return {m_document, m_cursor, m_element};
}

Element::Table Element::table() const {
  return {m_document, m_cursor, m_element};
}

Element::TableColumn Element::table_column() const {
  return {m_document, m_cursor, m_element};
}

Element::TableRow Element::table_row() const {
  return {m_document, m_cursor, m_element};
}

Element::TableCell Element::table_cell() const {
  return {m_document, m_cursor, m_element};
}

Element::Frame Element::frame() const {
  return {m_document, m_cursor, m_element};
}

Element::Rect Element::rect() const {
  return {m_document, m_cursor, m_element};
}

Element::Line Element::line() const {
  return {m_document, m_cursor, m_element};
}

Element::Circle Element::circle() const {
  return {m_document, m_cursor, m_element};
}

Element::CustomShape Element::custom_shape() const {
  return {m_document, m_cursor, m_element};
}

Element::Image Element::image() const {
  return {m_document, m_cursor, m_element};
}

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

} // namespace odr
