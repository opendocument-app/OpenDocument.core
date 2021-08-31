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
  return DocumentCursor(m_document, m_document->root_element());
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
  return Style(m_document, m_element, m_element->style(m_document));
}

Element::Extension::Extension(const internal::abstract::Document *document,
                              const internal::abstract::Element *element,
                              const void *extension)
    : m_document{document}, m_element{element}, m_extension{extension} {}

Element::Text::Text(const internal::abstract::Document *document,
                    const internal::abstract::Element *element,
                    const void *extension)
    : Extension(document, element, extension) {}

std::string Element::Text::value() const {
  return m_element->text(m_document)->value(m_document, m_element);
}

Element::Table::Table(const internal::abstract::Document *document,
                      const internal::abstract::Element *element,
                      const void *extension)
    : Extension(document, element, extension) {}

TableDimensions Element::Table::dimensions() const {
  return m_element->table(m_document)->dimensions(m_document, m_element);
}

Element::TableCell::TableCell(const internal::abstract::Document *document,
                              const internal::abstract::Element *element,
                              const void *extension)
    : Extension(document, element, extension) {}

TableDimensions Element::TableCell::span() const {
  return m_element->table_cell(m_document)->span(m_document, m_element);
}

Element::Image::Image(const internal::abstract::Document *document,
                      const internal::abstract::Element *element,
                      const void *extension)
    : Extension(document, element, extension) {}

bool Element::Image::internal() const {
  return m_element->image(m_document)->internal(m_document, m_element);
}

std::optional<File> Element::Image::file() const {
  return m_element->image(m_document)->file(m_document, m_element);
}

Element::Text Element::text() const {
  return Text(m_document, m_element, m_element->text(m_document));
}

Element::Table Element::table() const {
  return Table(m_document, m_element, m_element->table(m_document));
}

Element::TableCell Element::table_cell() const {
  return TableCell(m_document, m_element, m_element->table_cell(m_document));
}

Element::Image Element::image() const {
  return Image(m_document, m_element, m_element->image(m_document));
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

std::string DocumentCursor::text() const {
  auto element = m_cursor->element();
  return element->text(m_document.get())->value(m_document.get(), element);
}

bool DocumentCursor::move_to_master_page() {
  return m_cursor->slide()->move_to_master_page(m_cursor.get(),
                                                m_cursor->element());
}

TableDimensions DocumentCursor::table_dimensions() {
  auto element = m_cursor->element();
  return element->table(m_document.get())
      ->dimensions(m_document.get(), element);
}

bool DocumentCursor::move_to_first_table_column() {
  return m_cursor->table()->move_to_first_column(m_cursor.get(),
                                                 m_cursor->element());
}

bool DocumentCursor::move_to_first_table_row() {
  return m_cursor->table()->move_to_first_row(m_cursor.get(),
                                              m_cursor->element());
}

TableDimensions DocumentCursor::table_cell_span() {
  auto element = m_cursor->element();
  return element->table_cell(m_document.get())->span(m_document.get(), element);
}

bool DocumentCursor::image_internal() const {
  auto element = m_cursor->element();
  return element->image(m_document.get())->internal(m_document.get(), element);
}

std::optional<File> DocumentCursor::image_file() const {
  auto element = m_cursor->element();
  return element->image(m_document.get())->file(m_document.get(), element);
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
    const internal::abstract::Style *style,
    const internal::abstract::Element *element, void *property)
    : m_document{document}, m_style{style}, m_element{element}, m_property{
                                                                    property} {}

Property Style::DirectionalProperty::right() const {
  return Property(
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->right(m_document, m_style));
}

Property Style::DirectionalProperty::top() const {
  return Property(
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->top(m_document, m_style));
}

Property Style::DirectionalProperty::left() const {
  return Property(
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->left(m_document, m_style));
}

Property Style::DirectionalProperty::bottom() const {
  return Property(
      m_document, m_element, m_style,
      static_cast<internal::abstract::Style::DirectionalProperty *>(m_property)
          ->bottom(m_document, m_style));
}

Style::Extension::Extension(const internal::abstract::Document *document,
                            const internal::abstract::Element *element,
                            const internal::abstract::Style *style,
                            void *extension)
    : m_document{document}, m_element{element}, m_style{style},
      m_extension{extension} {}

Style::Text::Text(const internal::abstract::Document *document,
                  const internal::abstract::Element *element,
                  const internal::abstract::Style *style, void *extension)
    : Extension(document, element, style, extension) {}

Property Style::Text::font_name() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_name(m_document, m_style));
}

Property Style::Text::font_size() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_size(m_document, m_style));
}

Property Style::Text::font_weight() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_weight(m_document, m_style));
}

Property Style::Text::font_style() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_style(m_document, m_style));
}

Property Style::Text::font_underline() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_underline(m_document, m_style));
}

Property Style::Text::font_line_through() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_line_through(m_document, m_style));
}

Property Style::Text::font_shadow() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_shadow(m_document, m_style));
}

Property Style::Text::font_color() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->font_color(m_document, m_style));
}

Property Style::Text::background_color() const {
  return Property(m_document, m_element, m_style,
                  static_cast<internal::abstract::Style::Text *>(m_extension)
                      ->background_color(m_document, m_style));
}

Style::Paragraph::Paragraph(const internal::abstract::Document *document,
                            const internal::abstract::Element *element,
                            const internal::abstract::Style *style,
                            void *extension)
    : Extension(document, element, style, extension) {}

Style::Table::Table(const internal::abstract::Document *document,
                    const internal::abstract::Element *element,
                    const internal::abstract::Style *style, void *extension)
    : Extension(document, element, style, extension) {}

Style::TableColumn::TableColumn(const internal::abstract::Document *document,
                                const internal::abstract::Element *element,
                                const internal::abstract::Style *style,
                                void *extension)
    : Extension(document, element, style, extension) {}

Style::TableRow::TableRow(const internal::abstract::Document *document,
                          const internal::abstract::Element *element,
                          const internal::abstract::Style *style,
                          void *extension)
    : Extension(document, element, style, extension) {}

Style::TableCell::TableCell(const internal::abstract::Document *document,
                            const internal::abstract::Element *element,
                            const internal::abstract::Style *style,
                            void *extension)
    : Extension(document, element, style, extension) {}

Style::Graphic::Graphic(const internal::abstract::Document *document,
                        const internal::abstract::Element *element,
                        const internal::abstract::Style *style, void *extension)
    : Extension(document, element, style, extension) {}

Style::PageLayout::PageLayout(const internal::abstract::Document *document,
                              const internal::abstract::Element *element,
                              const internal::abstract::Style *style,
                              void *extension)
    : Extension(document, element, style, extension) {}

Style::Text Style::text() const {
  return Text(m_document, m_element, m_style, m_style->text(m_document));
}

Style::Paragraph Style::paragraph() const {
  return Paragraph(m_document, m_element, m_style,
                   m_style->paragraph(m_document));
}

Style::Table Style::table() const {
  return Table(m_document, m_element, m_style, m_style->table(m_document));
}

Style::TableColumn Style::table_column() const {
  return TableColumn(m_document, m_element, m_style,
                     m_style->table_column(m_document));
}

Style::TableRow Style::table_row() const {
  return TableRow(m_document, m_element, m_style,
                  m_style->table_row(m_document));
}

Style::TableCell Style::table_cell() const {
  return TableCell(m_document, m_element, m_style,
                   m_style->table_cell(m_document));
}

Style::Graphic Style::graphic() const {
  return Graphic(m_document, m_element, m_style, m_style->graphic(m_document));
}

Style::PageLayout Style::page_layout() const {
  return PageLayout(m_document, m_element, m_style,
                    m_style->page_layout(m_document));
}

} // namespace odr
