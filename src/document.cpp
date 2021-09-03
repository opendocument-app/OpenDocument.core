#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr {

namespace {
auto text_(const void *extension) {
  return static_cast<const internal::abstract::Element::Text *>(extension);
}
auto link_(const void *extension) {
  return static_cast<const internal::abstract::Element::Link *>(extension);
}
auto bookmark_(const void *extension) {
  return static_cast<const internal::abstract::Element::Bookmark *>(extension);
}
auto table_(const void *extension) {
  return static_cast<const internal::abstract::Element::Table *>(extension);
}
auto table_cell_(const void *extension) {
  return static_cast<const internal::abstract::Element::TableCell *>(extension);
}
auto frame_(const void *extension) {
  return static_cast<const internal::abstract::Element::Frame *>(extension);
}
auto rect_(const void *extension) {
  return static_cast<const internal::abstract::Element::Rect *>(extension);
}
auto line_(const void *extension) {
  return static_cast<const internal::abstract::Element::Line *>(extension);
}
auto circle_(const void *extension) {
  return static_cast<const internal::abstract::Element::Circle *>(extension);
}
auto custom_shape_(const void *extension) {
  return static_cast<const internal::abstract::Element::CustomShape *>(
      extension);
}
auto image_(const void *extension) {
  return static_cast<const internal::abstract::Element::Image *>(extension);
}

auto text_style_(void *extension) {
  return static_cast<internal::abstract::Style::Text *>(extension);
}
auto paragraph_style_(void *extension) {
  return static_cast<internal::abstract::Style::Paragraph *>(extension);
}
auto table_style_(void *extension) {
  return static_cast<internal::abstract::Style::Table *>(extension);
}
auto table_column_style_(void *extension) {
  return static_cast<internal::abstract::Style::TableColumn *>(extension);
}
auto table_row_style_(void *extension) {
  return static_cast<internal::abstract::Style::TableRow *>(extension);
}
auto table_cell_style_(void *extension) {
  return static_cast<internal::abstract::Style::TableCell *>(extension);
}
auto graphic_style_(void *extension) {
  return static_cast<internal::abstract::Style::Graphic *>(extension);
}
auto page_layout_(void *extension) {
  return static_cast<internal::abstract::Style::PageLayout *>(extension);
}

auto directional_property_(void *extension) {
  return static_cast<internal::abstract::Style::DirectionalProperty *>(
      extension);
}
} // namespace

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
  if (auto slide = m_cursor->slide()) {
    return slide->move_to_master_page(m_cursor.get(), m_cursor->element());
  }
  return false;
}

bool DocumentCursor::move_to_first_table_column() {
  if (auto table = m_cursor->table()) {
    return table->move_to_first_column(m_cursor.get(), m_cursor->element());
  }
  return false;
}

bool DocumentCursor::move_to_first_table_row() {
  if (auto table = m_cursor->table()) {
    return table->move_to_first_row(m_cursor.get(), m_cursor->element());
  }
  return false;
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
                 const internal::abstract::Element *element)
    : m_document{document}, m_element{element} {}

bool Element::operator==(const Element &rhs) const {
  return m_element->equals(m_document, *rhs.m_element);
}

bool Element::operator!=(const Element &rhs) const { return !operator==(rhs); }

Element::operator bool() const { return m_element; }

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

Element::Extension::operator bool() const { return m_extension; }

std::string Element::Text::value() const {
  return text_(m_extension) ? text_(m_extension)->value(m_document, m_element)
                            : "";
}

std::string Element::Link::href() const {
  return link_(m_extension) ? link_(m_extension)->href(m_document, m_element)
                            : "";
}

std::string Element::Bookmark::name() const {
  return bookmark_(m_extension)
             ? bookmark_(m_extension)->name(m_document, m_element)
             : "";
}

TableDimensions Element::Table::dimensions() const {
  return table_(m_extension)
             ? table_(m_extension)->dimensions(m_document, m_element)
             : TableDimensions();
}

TableDimensions Element::TableCell::span() const {
  return table_cell_(m_extension)
             ? table_cell_(m_extension)->span(m_document, m_element)
             : TableDimensions();
}

std::optional<std::string> Element::Frame::anchor_type() const {
  return frame_(m_extension)
             ? frame_(m_extension)->anchor_type(m_document, m_element)
             : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::x() const {
  return frame_(m_extension) ? frame_(m_extension)->x(m_document, m_element)
                             : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::y() const {
  return frame_(m_extension) ? frame_(m_extension)->y(m_document, m_element)
                             : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::width() const {
  return frame_(m_extension) ? frame_(m_extension)->width(m_document, m_element)
                             : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::height() const {
  return frame_(m_extension)
             ? frame_(m_extension)->height(m_document, m_element)
             : std::optional<std::string>();
}

std::optional<std::string> Element::Frame::z_index() const {
  return frame_(m_extension)
             ? frame_(m_extension)->z_index(m_document, m_element)
             : std::optional<std::string>();
}

std::string Element::Rect::x() const {
  return rect_(m_extension) ? rect_(m_extension)->x(m_document, m_element) : "";
}

std::string Element::Rect::y() const {
  return rect_(m_extension) ? rect_(m_extension)->y(m_document, m_element) : "";
}

std::string Element::Rect::width() const {
  return rect_(m_extension) ? rect_(m_extension)->width(m_document, m_element)
                            : "";
}

std::string Element::Rect::height() const {
  return rect_(m_extension) ? rect_(m_extension)->height(m_document, m_element)
                            : "";
}

std::string Element::Line::x1() const {
  return line_(m_extension) ? line_(m_extension)->x1(m_document, m_element)
                            : "";
}

std::string Element::Line::y1() const {
  return line_(m_extension) ? line_(m_extension)->y1(m_document, m_element)
                            : "";
}

std::string Element::Line::x2() const {
  return line_(m_extension) ? line_(m_extension)->x2(m_document, m_element)
                            : "";
}

std::string Element::Line::y2() const {
  return line_(m_extension) ? line_(m_extension)->y2(m_document, m_element)
                            : "";
}

std::string Element::Circle::x() const {
  return circle_(m_extension) ? circle_(m_extension)->x(m_document, m_element)
                              : "";
}

std::string Element::Circle::y() const {
  return circle_(m_extension) ? circle_(m_extension)->y(m_document, m_element)
                              : "";
}

std::string Element::Circle::width() const {
  return circle_(m_extension)
             ? circle_(m_extension)->width(m_document, m_element)
             : "";
}

std::string Element::Circle::height() const {
  return circle_(m_extension)
             ? circle_(m_extension)->height(m_document, m_element)
             : "";
}

std::optional<std::string> Element::CustomShape::x() const {
  return custom_shape_(m_extension)
             ? custom_shape_(m_extension)->x(m_document, m_element)
             : std::optional<std::string>();
}

std::optional<std::string> Element::CustomShape::y() const {
  return custom_shape_(m_extension)
             ? custom_shape_(m_extension)->y(m_document, m_element)
             : std::optional<std::string>();
}

std::string Element::CustomShape::width() const {
  return custom_shape_(m_extension)
             ? custom_shape_(m_extension)->width(m_document, m_element)
             : "";
}

std::string Element::CustomShape::height() const {
  return custom_shape_(m_extension)
             ? custom_shape_(m_extension)->height(m_document, m_element)
             : "";
}

bool Element::Image::internal() const {
  return image_(m_extension)
             ? image_(m_extension)->internal(m_document, m_element)
             : false;
}

std::optional<File> Element::Image::file() const {
  return image_(m_extension) ? image_(m_extension)->file(m_document, m_element)
                             : std::optional<File>();
}

std::string Element::Image::href() const {
  return image_(m_extension) ? image_(m_extension)->href(m_document, m_element)
                             : "";
}

Element::Text Element::text() const {
  return {m_document, m_element,
          m_element ? m_element->text(m_document) : nullptr};
}

Element::Link Element::link() const {
  return {m_document, m_element,
          m_element ? m_element->link(m_document) : nullptr};
}

Element::Bookmark Element::bookmark() const {
  return {m_document, m_element,
          m_element ? m_element->bookmark(m_document) : nullptr};
}

Element::Table Element::table() const {
  return {m_document, m_element,
          m_element ? m_element->table(m_document) : nullptr};
}

Element::TableCell Element::table_cell() const {
  return {m_document, m_element,
          m_element ? m_element->table_cell(m_document) : nullptr};
}

Element::Frame Element::frame() const {
  return {m_document, m_element,
          m_element ? m_element->frame(m_document) : nullptr};
}

Element::Rect Element::rect() const {
  return {m_document, m_element,
          m_element ? m_element->rect(m_document) : nullptr};
}

Element::Line Element::line() const {
  return {m_document, m_element,
          m_element ? m_element->line(m_document) : nullptr};
}

Element::Circle Element::circle() const {
  return {m_document, m_element,
          m_element ? m_element->circle(m_document) : nullptr};
}

Element::CustomShape Element::custom_shape() const {
  return {m_document, m_element,
          m_element ? m_element->custom_shape(m_document) : nullptr};
}

Element::Image Element::image() const {
  return {m_document, m_element,
          m_element ? m_element->image(m_document) : nullptr};
}

Style::Style(const internal::abstract::Document *document,
             const internal::abstract::Element *element,
             internal::abstract::Style *style)
    : m_document{document}, m_element{element}, m_style{style} {}

Style::operator bool() const { return m_style; }

Style::Extension::Extension(const internal::abstract::Document *document,
                            const internal::abstract::Element *element,
                            const internal::abstract::Style *style,
                            void *extension)
    : m_document{document}, m_element{element}, m_style{style},
      m_extension{extension} {}

Style::Extension::operator bool() const { return m_extension; }

Property Style::Text::font_name() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_name(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_size() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_size(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_weight() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_weight(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_style() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_style(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_underline() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_underline(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_line_through() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_line_through(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_shadow() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_shadow(m_document, m_style)
              : nullptr};
}

Property Style::Text::font_color() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->font_color(m_document, m_style)
              : nullptr};
}

Property Style::Text::background_color() const {
  return {m_document, m_element, m_style,
          text_style_(m_extension)
              ? text_style_(m_extension)->background_color(m_document, m_style)
              : nullptr};
}

Property Style::Paragraph::text_align() const {
  return {m_document, m_element, m_style,
          paragraph_style_(m_extension)
              ? paragraph_style_(m_extension)->text_align(m_document, m_style)
              : nullptr};
}

Style::DirectionalProperty Style::Paragraph::margin() const {
  return {m_document, m_element, m_style,
          paragraph_style_(m_extension)
              ? paragraph_style_(m_extension)->margin(m_document, m_style)
              : nullptr};
}

Property Style::Table::width() const {
  return {m_document, m_element, m_style,
          table_style_(m_extension)
              ? table_style_(m_extension)->width(m_document, m_style)
              : nullptr};
}

Property Style::TableColumn::width() const {
  return {m_document, m_element, m_style,
          table_column_style_(m_extension)
              ? table_column_style_(m_extension)->width(m_document, m_style)
              : nullptr};
}

Property Style::TableRow::height() const {
  return {m_document, m_element, m_style,
          table_row_style_(m_extension)
              ? table_row_style_(m_extension)->height(m_document, m_style)
              : nullptr};
}

Property Style::TableCell::vertical_align() const {
  return {
      m_document, m_element, m_style,
      table_cell_style_(m_extension)
          ? table_cell_style_(m_extension)->vertical_align(m_document, m_style)
          : nullptr};
}

Property Style::TableCell::background_color() const {
  return {m_document, m_element, m_style,
          table_cell_style_(m_extension)
              ? table_cell_style_(m_extension)
                    ->background_color(m_document, m_style)
              : nullptr};
}

Style::DirectionalProperty Style::TableCell::padding() const {
  return {m_document, m_element, m_style,
          table_cell_style_(m_extension)
              ? table_cell_style_(m_extension)->padding(m_document, m_style)
              : nullptr};
}

Style::DirectionalProperty Style::TableCell::border() const {
  return {m_document, m_element, m_style,
          table_cell_style_(m_extension)
              ? table_cell_style_(m_extension)->border(m_document, m_style)
              : nullptr};
}

Property Style::Graphic::stroke_width() const {
  return {m_document, m_element, m_style,
          graphic_style_(m_extension)
              ? graphic_style_(m_extension)->stroke_width(m_document, m_style)
              : nullptr};
}

Property Style::Graphic::stroke_color() const {
  return {m_document, m_element, m_style,
          graphic_style_(m_extension)
              ? graphic_style_(m_extension)->stroke_color(m_document, m_style)
              : nullptr};
}

Property Style::Graphic::fill_color() const {
  return {m_document, m_element, m_style,
          graphic_style_(m_extension)
              ? graphic_style_(m_extension)->fill_color(m_document, m_style)
              : nullptr};
}

Property Style::Graphic::vertical_align() const {
  return {m_document, m_element, m_style,
          graphic_style_(m_extension)
              ? graphic_style_(m_extension)->vertical_align(m_document, m_style)
              : nullptr};
}

Property Style::PageLayout::width() const {
  return {m_document, m_element, m_style,
          page_layout_(m_extension)
              ? page_layout_(m_extension)->width(m_document, m_style)
              : nullptr};
}

Property Style::PageLayout::height() const {
  return {m_document, m_element, m_style,
          page_layout_(m_extension)
              ? page_layout_(m_extension)->height(m_document, m_style)
              : nullptr};
}

Property Style::PageLayout::print_orientation() const {
  return {
      m_document, m_element, m_style,
      page_layout_(m_extension)
          ? page_layout_(m_extension)->print_orientation(m_document, m_style)
          : nullptr};
}

Style::DirectionalProperty Style::PageLayout::margin() const {
  return {m_document, m_element, m_style,
          page_layout_(m_extension)
              ? page_layout_(m_extension)->margin(m_document, m_style)
              : nullptr};
}

Style::Text Style::text() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->text(m_document) : nullptr};
}

Style::Paragraph Style::paragraph() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->paragraph(m_document) : nullptr};
}

Style::Table Style::table() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->table(m_document) : nullptr};
}

Style::TableColumn Style::table_column() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->table_column(m_document) : nullptr};
}

Style::TableRow Style::table_row() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->table_row(m_document) : nullptr};
}

Style::TableCell Style::table_cell() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->table_cell(m_document) : nullptr};
}

Style::Graphic Style::graphic() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->graphic(m_document) : nullptr};
}

Style::PageLayout Style::page_layout() const {
  return {m_document, m_element, m_style,
          m_style ? m_style->page_layout(m_document) : nullptr};
}

Property::Property(const internal::abstract::Document *document,
                   const internal::abstract::Element *element,
                   const internal::abstract::Style *style,
                   const internal::abstract::Property *property)
    : m_document{document}, m_element{element}, m_style{style}, m_property{
                                                                    property} {}

Property::operator bool() const { return m_property; }

std::optional<std::string> Property::value() const {
  if (!*this) {
    return {};
  }
  return m_property->value(m_document, m_element, m_style);
}

Style::DirectionalProperty::DirectionalProperty(
    const internal::abstract::Document *document,
    const internal::abstract::Element *element,
    const internal::abstract::Style *style, void *property)
    : m_document{document}, m_element{element}, m_style{style}, m_property{
                                                                    property} {}

Style::DirectionalProperty::operator bool() const { return m_property; }

Property Style::DirectionalProperty::right() const {
  return {m_document, m_element, m_style,
          directional_property_(m_property)
              ? directional_property_(m_property)->right(m_document, m_style)
              : nullptr};
}

Property Style::DirectionalProperty::top() const {
  return {m_document, m_element, m_style,
          directional_property_(m_property)
              ? directional_property_(m_property)->top(m_document, m_style)
              : nullptr};
}

Property Style::DirectionalProperty::left() const {
  return {m_document, m_element, m_style,
          directional_property_(m_property)
              ? directional_property_(m_property)->left(m_document, m_style)
              : nullptr};
}

Property Style::DirectionalProperty::bottom() const {
  return {m_document, m_element, m_style,
          directional_property_(m_property)
              ? directional_property_(m_property)->bottom(m_document, m_style)
              : nullptr};
}

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

} // namespace odr
