#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/abstract/table.h>
#include <internal/common/path.h>
#include <internal/util/map_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr {

namespace {
bool property_value_to_bool(const std::any &value) {
  if (value.type() == typeid(bool)) {
    return std::any_cast<bool>(value);
  }
  throw std::runtime_error("conversion to bool failed");
}

std::uint32_t property_value_to_uint32(const std::any &value) {
  if (value.type() == typeid(std::uint32_t)) {
    return std::any_cast<std::uint32_t>(value);
  }
  throw std::runtime_error("conversion to uint32 failed");
}

const char *property_value_to_cstr(const std::any &value) {
  if (value.type() == typeid(const char *)) {
    return std::any_cast<const char *>(value);
  }
  throw std::runtime_error("conversion to cstr failed");
}

std::string property_value_to_string(const std::any &value) {
  if (value.type() == typeid(std::string)) {
    return std::any_cast<std::string>(value);
  } else if (value.type() == typeid(const char *)) {
    return std::any_cast<const char *>(value);
  }
  throw std::runtime_error("conversion to string failed");
}
} // namespace

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(std::uint32_t rows, std::uint32_t columns)
    : rows{rows}, columns{columns} {}

PageStyle::PageStyle() = default;

PageStyle::PageStyle(std::shared_ptr<const internal::abstract::Document> impl,
                     const std::uint64_t id)
    : m_impl{std::move(impl)}, m_id{id} {}

bool PageStyle::operator==(const PageStyle &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id;
}

bool PageStyle::operator!=(const PageStyle &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id;
}

PageStyle::operator bool() const { return m_impl.operator bool() && m_id != 0; }

ElementPropertyValue PageStyle::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue PageStyle::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
}

ElementPropertyValue PageStyle::margin_top() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_TOP);
}

ElementPropertyValue PageStyle::margin_bottom() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_BOTTOM);
}

ElementPropertyValue PageStyle::margin_left() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_LEFT);
}

ElementPropertyValue PageStyle::margin_right() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_RIGHT);
}

Element::Element() = default;

Element::Element(std::shared_ptr<const internal::abstract::Document> impl,
                 const std::uint64_t id)
    : m_impl{std::move(impl)}, m_id{id} {}

bool Element::operator==(const Element &rhs) const { return m_id == rhs.m_id; }

bool Element::operator!=(const Element &rhs) const { return m_id != rhs.m_id; }

Element::operator bool() const { return m_id != 0; }

ElementType Element::type() const {
  if (!m_impl) {
    return ElementType::NONE;
  }
  return m_impl->element_type(m_id);
}

Element Element::parent() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_parent(m_id));
}

Element Element::first_child() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_first_child(m_id));
}

Element Element::previous_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_previous_sibling(m_id));
}

Element Element::next_sibling() const {
  if (!m_impl) {
    return Element();
  }
  return Element(m_impl, m_impl->element_next_sibling(m_id));
}

ElementRange Element::children() const { return ElementRange(first_child()); }

ElementPropertyValue Element::property(ElementProperty property) const {
  return ElementPropertyValue(m_impl, m_id, property);
}

Slide Element::slide() const { return Slide(m_impl, m_id); }

Sheet Element::sheet() const { return Sheet(m_impl, m_id); }

Page Element::page() const { return Page(m_impl, m_id); }

Text Element::text() const { return Text(m_impl, m_id); }

Element Element::line_break() const {
  if (type() != ElementType::LINE_BREAK) {
    return {};
  }
  return *this;
}

Element Element::page_break() const {
  if (type() != ElementType::PAGE_BREAK) {
    return {};
  }
  return *this;
}

Paragraph Element::paragraph() const { return Paragraph(m_impl, m_id); }

Span Element::span() const { return Span(m_impl, m_id); }

Link Element::link() const { return Link(m_impl, m_id); }

Bookmark Element::bookmark() const { return Bookmark(m_impl, m_id); }

List Element::list() const { return List(m_impl, m_id); }

ListItem Element::list_item() const { return ListItem(m_impl, m_id); }

Table Element::table() const { return Table(m_impl, m_id); }

Frame Element::frame() const { return Frame(m_impl, m_id); }

Image Element::image() const { return Image(m_impl, m_id); }

Rect Element::rect() const { return Rect(m_impl, m_id); }

Line Element::line() const { return Line(m_impl, m_id); }

Circle Element::circle() const { return Circle(m_impl, m_id); }

template <typename E>
ElementIterator<E>::ElementIterator(E element)
    : m_element{std::move(element)} {}

template <typename E> ElementIterator<E> &ElementIterator<E>::operator++() {
  m_element = m_element.next_sibling();
  return *this;
}

template <typename E> ElementIterator<E> ElementIterator<E>::operator++(int) & {
  ElementIterator<E> result = *this;
  operator++();
  return result;
}

template <typename E> E &ElementIterator<E>::operator*() { return m_element; }

template <typename E> E *ElementIterator<E>::operator->() { return &m_element; }

template <typename E>
bool ElementIterator<E>::operator==(const ElementIterator<E> &rhs) const {
  return m_element == rhs.m_element;
}

template <typename E>
bool ElementIterator<E>::operator!=(const ElementIterator<E> &rhs) const {
  return m_element != rhs.m_element;
}

template class ElementIterator<Element>;
template class ElementIterator<Slide>;
template class ElementIterator<Sheet>;
template class ElementIterator<Page>;
template class ElementIterator<TableColumn>;
template class ElementIterator<TableRow>;
template class ElementIterator<TableCell>;

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate()
    : ElementRangeTemplate({}, {}) {}

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate(E begin)
    : ElementRangeTemplate(std::move(begin), {}) {}

template <typename E>
ElementRangeTemplate<E>::ElementRangeTemplate(E begin, E end)
    : m_begin{std::move(begin)}, m_end{std::move(end)} {}

template <typename E>
ElementIterator<E> ElementRangeTemplate<E>::begin() const {
  return ElementIterator<E>(m_begin);
}

template <typename E> ElementIterator<E> ElementRangeTemplate<E>::end() const {
  return ElementIterator<E>(m_end);
}

template <typename E> E ElementRangeTemplate<E>::front() const {
  return m_begin;
}

template class ElementRangeTemplate<Element>;
template class ElementRangeTemplate<Slide>;
template class ElementRangeTemplate<Sheet>;
template class ElementRangeTemplate<Page>;
template class ElementRangeTemplate<TableColumn>;
template class ElementRangeTemplate<TableRow>;
template class ElementRangeTemplate<TableCell>;

Slide::Slide() = default;

Slide::Slide(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id) {}

Slide Slide::previous_sibling() const {
  return Element::previous_sibling().slide();
}

Slide Slide::next_sibling() const { return Element::next_sibling().slide(); }

std::string Slide::name() const {
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::NAME));
}

std::string Slide::notes() const {
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::NOTES));
}

PageStyle Slide::page_style() const { return PageStyle(m_impl, m_id); }

Sheet::Sheet() = default;

Sheet::Sheet(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id) {}

Sheet Sheet::previous_sibling() const {
  return Element::previous_sibling().sheet();
}

Sheet Sheet::next_sibling() const { return Element::next_sibling().sheet(); }

std::string Sheet::name() const {
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::NAME));
}

Table Sheet::table() const {
  if (!m_impl) {
    return Table();
  }
  return Table(m_impl, m_id); // TODO
}

Page::Page() = default;

Page::Page(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

Page Page::previous_sibling() const {
  return Element::previous_sibling().page();
}

Page Page::next_sibling() const { return Element::next_sibling().page(); }

std::string Page::name() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::NAME));
}

PageStyle Page::page_style() const { return PageStyle(m_impl, m_id); }

Text::Text() = default;

Text::Text(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Text::string() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::TEXT));
}

Paragraph::Paragraph() = default;

Paragraph::Paragraph(std::shared_ptr<const internal::abstract::Document> impl,
                     const std::uint64_t id)
    : Element(std::move(impl), id) {}

Span::Span() = default;

Span::Span(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

Link::Link() = default;

Link::Link(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Link::href() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::HREF));
}

Bookmark::Bookmark() = default;

Bookmark::Bookmark(std::shared_ptr<const internal::abstract::Document> impl,
                   const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Bookmark::name() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::NAME));
}

List::List() = default;

List::List(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

ListItem::ListItem() = default;

ListItem::ListItem(std::shared_ptr<const internal::abstract::Document> impl,
                   const std::uint64_t id)
    : Element(std::move(impl), id) {}

Table::Table() = default;

Table::Table(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id), m_table{m_impl->table(id)} {}

TableDimensions Table::dimensions() const {
  if (!m_impl) {
    return TableDimensions(); // TODO
  }
  // TODO limit?
  return m_table->dimensions();
}

TableColumnRange Table::columns() const {
  if (!m_impl) {
    return TableColumnRange();
  }
  return TableColumnRange(); // TODO
}

TableRowRange Table::rows() const {
  if (!m_impl) {
    return TableRowRange();
  }
  return TableRowRange(); // TODO
}

TableColumn::TableColumn() = default;

TableColumn::TableColumn(std::shared_ptr<const internal::abstract::Table> impl,
                         const std::uint32_t column)
    : m_impl{std::move(impl)}, m_column{column} {}

bool TableColumn::operator==(const TableColumn &rhs) const {
  return m_impl == rhs.m_impl && m_column == rhs.m_column;
}

bool TableColumn::operator!=(const TableColumn &rhs) const {
  return m_impl != rhs.m_impl || m_column != rhs.m_column;
}

TableColumn::operator bool() const { return m_impl.operator bool(); }

TableColumn TableColumn::previous_sibling() const {
  return TableColumn(m_impl, m_column - 1);
}

TableColumn TableColumn::next_sibling() const {
  return TableColumn(m_impl, m_column + 1);
}

TableColumnPropertyValue
TableColumn::property(const ElementProperty property) const {
  return TableColumnPropertyValue(m_impl, m_column, property);
}

TableRow::TableRow() = default;

TableRow::TableRow(std::shared_ptr<const internal::abstract::Table> impl,
                   const std::uint32_t row)
    : m_impl{std::move(impl)}, m_row{row} {}

bool TableRow::operator==(const TableRow &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row;
}

bool TableRow::operator!=(const TableRow &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row;
}

TableRow::operator bool() const { return m_impl.operator bool(); }

TableCell TableRow::first_child() const {
  if (!m_impl) {
    return TableCell();
  }
  return TableCell(m_impl, m_row, 0);
}

TableRow TableRow::previous_sibling() const {
  return TableRow(m_impl, m_row - 1);
}

TableRow TableRow::next_sibling() const { return TableRow(m_impl, m_row + 1); }

TableCellRange TableRow::cells() const { return TableCellRange(first_child()); }

TableRowPropertyValue TableRow::property(const ElementProperty property) const {
  return TableRowPropertyValue(m_impl, m_row, property);
}

TableCell::TableCell() = default;

TableCell::TableCell(std::shared_ptr<const internal::abstract::Table> impl,
                     const std::uint32_t row, const std::uint32_t column)
    : m_impl{std::move(impl)}, m_row{row}, m_column{column} {}

bool TableCell::operator==(const TableCell &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row && m_column == rhs.m_column;
}

bool TableCell::operator!=(const TableCell &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row || m_column != rhs.m_column;
}

TableCell::operator bool() const { return m_impl.operator bool(); }

TableCell TableCell::previous_sibling() const {
  return TableCell(m_impl, m_row, m_column - 1);
}

TableCell TableCell::next_sibling() const {
  return TableCell(m_impl, m_row, m_column + 1);
}

ElementRange TableCell::children() const {
  return ElementRange(
      Element(nullptr, m_impl->cell_first_child(m_row, m_column)));
}

TableCellPropertyValue
TableCell::property(const ElementProperty property) const {
  return TableCellPropertyValue(m_impl, m_row, m_column, property);
}

std::uint32_t TableCell::row_span() const {
  if (!m_impl) {
    return 0;
  }
  return property_value_to_uint32(
      m_impl->cell_properties(m_row, m_column)
          .at(ElementProperty::TABLE_CELL_ROW_SPAN));
}

std::uint32_t TableCell::column_span() const {
  if (!m_impl) {
    return 0;
  }
  return property_value_to_uint32(
      m_impl->cell_properties(m_row, m_column)
          .at(ElementProperty::TABLE_CELL_COLUMN_SPAN));
}

Frame::Frame() = default;

Frame::Frame(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Frame::anchor_type() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::ANCHOR_TYPE));
}

std::string Frame::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::WIDTH));
}

std::string Frame::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::HEIGHT));
}

std::string Frame::z_index() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::Z_INDEX));
}

Image::Image() = default;

Image::Image(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id) {}

bool Image::internal() const {
  if (!m_impl || !m_impl->files()) {
    return false;
  }

  try {
    const std::string href = this->href();
    const internal::common::Path path{href};

    return m_impl->files()->is_file(path);
  } catch (...) {
  }

  return false;
}

std::string Image::href() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::HREF));
}

File Image::image_file() const {
  if (!m_impl) {
    // TODO throw; there is no "empty" file
    return File(std::shared_ptr<internal::abstract::File>());
  }

  if (!internal()) {
    // TODO throw / support external files
    return File(std::shared_ptr<internal::abstract::File>());
  }

  const std::string href = this->href();
  const internal::common::Path path{href};

  return File(m_impl->files()->open(path));
}

Rect::Rect() = default;

Rect::Rect(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Rect::x() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::X));
}

std::string Rect::y() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::Y));
}

std::string Rect::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::WIDTH));
}

std::string Rect::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::HEIGHT));
}

Line::Line() = default;

Line::Line(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Line::x1() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::X1));
}

std::string Line::y1() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::Y1));
}

std::string Line::x2() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::X2));
}

std::string Line::y2() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::Y2));
}

Circle::Circle() = default;

Circle::Circle(std::shared_ptr<const internal::abstract::Document> impl,
               const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Circle::x() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::X));
}

std::string Circle::y() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::Y));
}

std::string Circle::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::WIDTH));
}

std::string Circle::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_properties(m_id).at(ElementProperty::HEIGHT));
}

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_impl{std::move(document)} {
  if (!m_impl) {
    throw std::runtime_error("document is null");
  }
}

bool Document::editable() const noexcept { return m_impl->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_impl->savable(encrypted);
}

void Document::save(const std::string &path) const { m_impl->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_impl->save(path, password.c_str());
}

DocumentType Document::document_type() const noexcept {
  return m_impl->document_type();
}

Element Document::root() const {
  return Element(m_impl, m_impl->root_element());
}

TextDocument Document::text_document() const { return TextDocument(m_impl); }

Presentation Document::presentation() const { return Presentation(m_impl); }

Spreadsheet Document::spreadsheet() const { return Spreadsheet(m_impl); }

Drawing Document::drawing() const { return Drawing(m_impl); }

TextDocument::TextDocument(
    std::shared_ptr<internal::abstract::Document> text_document)
    : Document(std::move(text_document)) {
  if (m_impl->document_type() != DocumentType::TEXT) {
    throw std::runtime_error("not a text document");
  }
}

ElementRange TextDocument::content() const { return root().children(); }

PageStyle TextDocument::page_style() const {
  return PageStyle(m_impl, m_impl->root_element());
}

Presentation::Presentation(
    std::shared_ptr<internal::abstract::Document> presentation)
    : Document(std::move(presentation)) {
  if (m_impl->document_type() != DocumentType::PRESENTATION) {
    throw std::runtime_error("not a presentation");
  }
}

std::uint32_t Presentation::slide_count() const {
  const auto range = slides();
  return std::distance(std::begin(range), std::end(range));
}

SlideRange Presentation::slides() const {
  return SlideRange(Slide(m_impl, m_impl->first_entry_element()));
}

Spreadsheet::Spreadsheet(
    std::shared_ptr<internal::abstract::Document> spreadsheet)
    : Document(std::move(spreadsheet)) {
  if (m_impl->document_type() != DocumentType::SPREADSHEET) {
    throw std::runtime_error("not a spreadsheet");
  }
}

std::uint32_t Spreadsheet::sheet_count() const {
  const auto range = sheets();
  return std::distance(std::begin(range), std::end(range));
}

SheetRange Spreadsheet::sheets() const {
  return SheetRange(Sheet(m_impl, m_impl->first_entry_element()));
}

Drawing::Drawing(std::shared_ptr<internal::abstract::Document> drawing)
    : Document(std::move(drawing)) {
  if (m_impl->document_type() != DocumentType::DRAWING) {
    throw std::runtime_error("not a drawing");
  }
}

std::uint32_t Drawing::page_count() const {
  const auto range = pages();
  return std::distance(std::begin(range), std::end(range));
}

PageRange Drawing::pages() const {
  return PageRange(Page(m_impl, m_impl->first_entry_element()));
}

std::string PropertyValue::get_string() const {
  return property_value_to_string(get());
}

std::uint32_t PropertyValue::get_uint32() const {
  return property_value_to_uint32(get());
}

bool PropertyValue::get_bool() const { return property_value_to_bool(get()); }

const char *PropertyValue::get_optional_string() const {
  return property_value_to_cstr(get());
}

void PropertyValue::set_string(const std::string &value) const {
  set(value.c_str());
}

ElementPropertyValue::ElementPropertyValue() = default;

ElementPropertyValue::ElementPropertyValue(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id, const ElementProperty property)
    : m_impl{std::move(impl)}, m_id{id}, m_property{property} {}

bool ElementPropertyValue::operator==(const ElementPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id &&
         m_property == rhs.m_property;
}

bool ElementPropertyValue::operator!=(const ElementPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id ||
         m_property != rhs.m_property;
}

ElementPropertyValue::operator bool() const {
  return m_impl.operator bool() && m_id != 0 && get().has_value();
}

std::any ElementPropertyValue::get() const {
  auto properties = m_impl->element_properties(m_id);
  return internal::util::map::lookup_map_default(properties, m_property,
                                                 std::any());
}

void ElementPropertyValue::set(const std::any &value) const {
  m_impl->update_element_properties(m_id, {{m_property, value}});
}

void ElementPropertyValue::remove() const {
  m_impl->update_element_properties(m_id, {{m_property, {}}});
}

TableColumnPropertyValue::TableColumnPropertyValue() = default;

TableColumnPropertyValue::TableColumnPropertyValue(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t column, const ElementProperty property)
    : m_impl{std::move(impl)}, m_column{column}, m_property{property} {}

bool TableColumnPropertyValue::operator==(
    const TableColumnPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_column == rhs.m_column &&
         m_property == rhs.m_property;
}

bool TableColumnPropertyValue::operator!=(
    const TableColumnPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_column != rhs.m_column ||
         m_property != rhs.m_property;
}

TableColumnPropertyValue::operator bool() const {
  return m_impl.operator bool();
}

std::any TableColumnPropertyValue::get() const {
  return m_impl->column_properties(m_column).at(m_property);
}

void TableColumnPropertyValue::set(const std::any &value) const {
  throw UnsupportedOperation(); // TODO
}

void TableColumnPropertyValue::remove() const {
  throw UnsupportedOperation(); // TODO
}

TableRowPropertyValue::TableRowPropertyValue() = default;

TableRowPropertyValue::TableRowPropertyValue(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t row, const ElementProperty property)
    : m_impl{std::move(impl)}, m_row{row}, m_property{property} {}

bool TableRowPropertyValue::operator==(const TableRowPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row &&
         m_property == rhs.m_property;
}

bool TableRowPropertyValue::operator!=(const TableRowPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row ||
         m_property != rhs.m_property;
}

TableRowPropertyValue::operator bool() const { return m_impl.operator bool(); }

std::any TableRowPropertyValue::get() const {
  return m_impl->row_properties(m_row).at(m_property);
}

void TableRowPropertyValue::set(const std::any &value) const {
  throw UnsupportedOperation(); // TODO
}

void TableRowPropertyValue::remove() const {
  throw UnsupportedOperation(); // TODO
}

TableCellPropertyValue::TableCellPropertyValue() = default;

TableCellPropertyValue::TableCellPropertyValue(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t row, const std::uint32_t column,
    const ElementProperty property)
    : m_impl{std::move(impl)}, m_row{row}, m_column{column}, m_property{
                                                                 property} {}

bool TableCellPropertyValue::operator==(
    const TableCellPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row &&
         m_column == rhs.m_column && m_property == rhs.m_property;
}

bool TableCellPropertyValue::operator!=(
    const TableCellPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row ||
         m_column != rhs.m_column || m_property != rhs.m_property;
}

TableCellPropertyValue::operator bool() const { return m_impl.operator bool(); }

std::any TableCellPropertyValue::get() const {
  return m_impl->cell_properties(m_row, m_column).at(m_property);
}

void TableCellPropertyValue::set(const std::any &value) const {
  throw UnsupportedOperation(); // TODO
}

void TableCellPropertyValue::remove() const {
  throw UnsupportedOperation(); // TODO
}

} // namespace odr
