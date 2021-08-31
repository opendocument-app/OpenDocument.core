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

Element::Element(const internal::abstract::Document *document,
                 const internal::abstract::Element *element)
    : m_document{document}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element->equals(m_document, *rhs.m_element);
}

bool Element::operator!=(const Element &rhs) const { return !operator==(rhs); }

ElementType Element::type() const { return m_element->type(m_document); }

std::optional<std::string> Element::style_name() const {
  return m_element->style_name(m_document);
}

Style Element::style() const {
  return {m_document, m_element, m_element->style(m_document)};
}

Element::Extension::Extension(const internal::abstract::Document *document,
                              const internal::abstract::Element *element,
                              const void *extension)
    : m_document{document}, m_element{element}, m_extension{extension} {}

std::string Element::Text::value() const {
  return static_cast<const internal::abstract::Element::Text *>(m_extension)
      ->value(m_document, m_element);
}

std::string Element::Link::href() const {
  return static_cast<const internal::abstract::Element::Link *>(m_extension)
      ->href(m_document, m_element);
}

std::string Element::Bookmark::name() const {
  return static_cast<const internal::abstract::Element::Bookmark *>(m_extension)
      ->name(m_document, m_element);
}

TableDimensions Element::Table::dimensions() const {
  return static_cast<const internal::abstract::Element::Table *>(m_extension)
      ->dimensions(m_document, m_element);
}

TableDimensions Element::TableCell::span() const {
  return static_cast<const internal::abstract::Element::TableCell *>(
             m_extension)
      ->span(m_document, m_element);
}

std::optional<std::string> Element::Frame::anchor_type() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->anchor_type(m_document, m_element);
}

std::optional<std::string> Element::Frame::x() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->x(m_document, m_element);
}

std::optional<std::string> Element::Frame::y() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->y(m_document, m_element);
}

std::optional<std::string> Element::Frame::width() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->width(m_document, m_element);
}

std::optional<std::string> Element::Frame::height() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->height(m_document, m_element);
}

std::optional<std::string> Element::Frame::z_index() const {
  return static_cast<const internal::abstract::Element::Frame *>(m_extension)
      ->z_index(m_document, m_element);
}

std::string Element::Rect::x() const {
  return static_cast<const internal::abstract::Element::Rect *>(m_extension)
      ->x(m_document, m_element);
}

std::string Element::Rect::y() const {
  return static_cast<const internal::abstract::Element::Rect *>(m_extension)
      ->y(m_document, m_element);
}

std::string Element::Rect::width() const {
  return static_cast<const internal::abstract::Element::Rect *>(m_extension)
      ->width(m_document, m_element);
}

std::string Element::Rect::height() const {
  return static_cast<const internal::abstract::Element::Rect *>(m_extension)
      ->height(m_document, m_element);
}

std::string Element::Line::x1() const {
  return static_cast<const internal::abstract::Element::Line *>(m_extension)
      ->x1(m_document, m_element);
}

std::string Element::Line::y1() const {
  return static_cast<const internal::abstract::Element::Line *>(m_extension)
      ->y1(m_document, m_element);
}

std::string Element::Line::x2() const {
  return static_cast<const internal::abstract::Element::Line *>(m_extension)
      ->x2(m_document, m_element);
}

std::string Element::Line::y2() const {
  return static_cast<const internal::abstract::Element::Line *>(m_extension)
      ->y2(m_document, m_element);
}

std::string Element::Circle::x() const {
  return static_cast<const internal::abstract::Element::Circle *>(m_extension)
      ->x(m_document, m_element);
}

std::string Element::Circle::y() const {
  return static_cast<const internal::abstract::Element::Circle *>(m_extension)
      ->y(m_document, m_element);
}

std::string Element::Circle::width() const {
  return static_cast<const internal::abstract::Element::Circle *>(m_extension)
      ->width(m_document, m_element);
}

std::string Element::Circle::height() const {
  return static_cast<const internal::abstract::Element::Circle *>(m_extension)
      ->height(m_document, m_element);
}

std::optional<std::string> Element::CustomShape::x() const {
  return static_cast<const internal::abstract::Element::CustomShape *>(
             m_extension)
      ->x(m_document, m_element);
}

std::optional<std::string> Element::CustomShape::y() const {
  return static_cast<const internal::abstract::Element::CustomShape *>(
             m_extension)
      ->y(m_document, m_element);
}

std::string Element::CustomShape::width() const {
  return static_cast<const internal::abstract::Element::CustomShape *>(
             m_extension)
      ->width(m_document, m_element);
}

std::string Element::CustomShape::height() const {
  return static_cast<const internal::abstract::Element::CustomShape *>(
             m_extension)
      ->height(m_document, m_element);
}

bool Element::Image::internal() const {
  return static_cast<const internal::abstract::Element::Image *>(m_extension)
      ->internal(m_document, m_element);
}

std::optional<File> Element::Image::file() const {
  return static_cast<const internal::abstract::Element::Image *>(m_extension)
      ->file(m_document, m_element);
}

std::string Element::Image::href() const {
  return static_cast<const internal::abstract::Element::Image *>(m_extension)
      ->href(m_document, m_element);
}

Element::Text Element::text() const {
  return {m_document, m_element, m_element->text(m_document)};
}

Element::Link Element::link() const {
  return {m_document, m_element, m_element->link(m_document)};
}

Element::Bookmark Element::bookmark() const {
  return {m_document, m_element, m_element->bookmark(m_document)};
}

Element::Table Element::table() const {
  return {m_document, m_element, m_element->table(m_document)};
}

Element::TableCell Element::table_cell() const {
  return {m_document, m_element, m_element->table_cell(m_document)};
}

Element::Frame Element::frame() const {
  return {m_document, m_element, m_element->frame(m_document)};
}

Element::Rect Element::rect() const {
  return {m_document, m_element, m_element->rect(m_document)};
}

Element::Line Element::line() const {
  return {m_document, m_element, m_element->line(m_document)};
}

Element::Circle Element::circle() const {
  return {m_document, m_element, m_element->circle(m_document)};
}

Element::CustomShape Element::custom_shape() const {
  return {m_document, m_element, m_element->custom_shape(m_document)};
}

Element::Image Element::image() const {
  return {m_document, m_element, m_element->image(m_document)};
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
  return m_cursor->slide()->move_to_master_page(m_cursor.get(),
                                                m_cursor->element());
}

bool DocumentCursor::move_to_first_table_column() {
  return m_cursor->table()->move_to_first_column(m_cursor.get(),
                                                 m_cursor->element());
}

bool DocumentCursor::move_to_first_table_row() {
  return m_cursor->table()->move_to_first_row(m_cursor.get(),
                                              m_cursor->element());
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

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

Property::Property(const internal::abstract::Document *document,
                   const internal::abstract::Element *element,
                   const internal::abstract::Style *style,
                   const internal::abstract::Property *property)
    : m_document{document}, m_element{element}, m_style{style}, m_property{
                                                                    property} {}

std::optional<std::string> Property::value() const {
  return m_property->value(m_document, m_element, m_style);
}

Style::Style(const internal::abstract::Document *document,
             const internal::abstract::Element *element,
             internal::abstract::Style *style)
    : m_document{document}, m_element{element}, m_style{style} {}

Style::DirectionalProperty::DirectionalProperty(
    const internal::abstract::Document *document,
    const internal::abstract::Element *element,
    const internal::abstract::Style *style, void *property)
    : m_document{document}, m_element{element}, m_style{style}, m_property{
                                                                    property} {}

Property Style::DirectionalProperty::right() const {
  return {
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->right(m_document, m_style)};
}

Property Style::DirectionalProperty::top() const {
  return {
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->top(m_document, m_style)};
}

Property Style::DirectionalProperty::left() const {
  return {
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->left(m_document, m_style)};
}

Property Style::DirectionalProperty::bottom() const {
  return {
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->bottom(m_document, m_style)};
}

Style::Extension::Extension(const internal::abstract::Document *document,
                            const internal::abstract::Element *element,
                            const internal::abstract::Style *style,
                            void *extension)
    : m_document{document}, m_element{element}, m_style{style},
      m_extension{extension} {}

Property Style::Text::font_name() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_name(m_document, m_style)};
}

Property Style::Text::font_size() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_size(m_document, m_style)};
}

Property Style::Text::font_weight() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_weight(m_document, m_style)};
}

Property Style::Text::font_style() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_style(m_document, m_style)};
}

Property Style::Text::font_underline() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_underline(m_document, m_style)};
}

Property Style::Text::font_line_through() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_line_through(m_document, m_style)};
}

Property Style::Text::font_shadow() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_shadow(m_document, m_style)};
}

Property Style::Text::font_color() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->font_color(m_document, m_style)};
}

Property Style::Text::background_color() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Text *>(m_extension)
              ->background_color(m_document, m_style)};
}

Property Style::Paragraph::text_align() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Paragraph *>(m_extension)
              ->text_align(m_document, m_style)};
}

Style::DirectionalProperty Style::Paragraph::margin() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Paragraph *>(m_extension)
              ->margin(m_document, m_style)};
}

Property Style::Table::width() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Table *>(m_extension)
              ->width(m_document, m_style)};
}

Property Style::TableColumn::width() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableColumn *>(m_extension)
              ->width(m_document, m_style)};
}

Property Style::TableRow::height() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableRow *>(m_extension)
              ->height(m_document, m_style)};
}

Property Style::TableCell::vertical_align() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableCell *>(m_extension)
              ->vertical_align(m_document, m_style)};
}

Property Style::TableCell::background_color() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableCell *>(m_extension)
              ->background_color(m_document, m_style)};
}

Style::DirectionalProperty Style::TableCell::padding() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableCell *>(m_extension)
              ->padding(m_document, m_style)};
}

Style::DirectionalProperty Style::TableCell::border() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::TableCell *>(m_extension)
              ->border(m_document, m_style)};
}

Property Style::Graphic::stroke_width() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Graphic *>(m_extension)
              ->stroke_width(m_document, m_style)};
}

Property Style::Graphic::stroke_color() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Graphic *>(m_extension)
              ->stroke_color(m_document, m_style)};
}

Property Style::Graphic::fill_color() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Graphic *>(m_extension)
              ->fill_color(m_document, m_style)};
}

Property Style::Graphic::vertical_align() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::Graphic *>(m_extension)
              ->vertical_align(m_document, m_style)};
}

Property Style::PageLayout::width() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::PageLayout *>(m_extension)
              ->width(m_document, m_style)};
}

Property Style::PageLayout::height() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::PageLayout *>(m_extension)
              ->height(m_document, m_style)};
}

Property Style::PageLayout::print_orientation() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::PageLayout *>(m_extension)
              ->print_orientation(m_document, m_style)};
}

Style::DirectionalProperty Style::PageLayout::margin() const {
  return {m_document, m_element, m_style,
          static_cast<internal::abstract::Style::PageLayout *>(m_extension)
              ->margin(m_document, m_style)};
}

Style::Text Style::text() const {
  return {m_document, m_element, m_style, m_style->text(m_document)};
}

Style::Paragraph Style::paragraph() const {
  return {m_document, m_element, m_style, m_style->paragraph(m_document)};
}

Style::Table Style::table() const {
  return {m_document, m_element, m_style, m_style->table(m_document)};
}

Style::TableColumn Style::table_column() const {
  return {m_document, m_element, m_style, m_style->table_column(m_document)};
}

Style::TableRow Style::table_row() const {
  return {m_document, m_element, m_style, m_style->table_row(m_document)};
}

Style::TableCell Style::table_cell() const {
  return {m_document, m_element, m_style, m_style->table_cell(m_document)};
}

Style::Graphic Style::graphic() const {
  return {m_document, m_element, m_style, m_style->graphic(m_document)};
}

Style::PageLayout Style::page_layout() const {
  return {m_document, m_element, m_style, m_style->page_layout(m_document)};
}

} // namespace odr
