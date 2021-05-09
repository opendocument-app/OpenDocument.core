#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/abstract/table.h>
#include <internal/common/path.h>
#include <internal/common/table_range.h>
#include <internal/util/map_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr {

namespace {
std::optional<bool> property_value_to_bool(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
  if (value.type() == typeid(bool)) {
    return std::any_cast<bool>(value);
  }
  throw std::runtime_error("conversion to bool failed");
}

std::optional<std::uint32_t> property_value_to_uint32(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
  if (value.type() == typeid(std::uint32_t)) {
    return std::any_cast<std::uint32_t>(value);
  }
  throw std::runtime_error("conversion to uint32 failed");
}

std::optional<std::string> property_value_to_string(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
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

std::string PropertyValue::get_string() const {
  return *property_value_to_string(get());
}

std::uint32_t PropertyValue::get_uint32() const {
  return *property_value_to_uint32(get());
}

bool PropertyValue::get_bool() const { return *property_value_to_bool(get()); }

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
  return m_impl.operator bool() && get().has_value();
}

std::any TableColumnPropertyValue::get() const {
  auto properties = m_impl->column_properties(m_column);
  return internal::util::map::lookup_map_default(properties, m_property,
                                                 std::any());
}

void TableColumnPropertyValue::set(const std::any &value) const {
  m_impl->update_column_properties(m_column, {{m_property, value}});
}

void TableColumnPropertyValue::remove() const {
  m_impl->update_column_properties(m_column, {{m_property, {}}});
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

TableRowPropertyValue::operator bool() const {
  return m_impl.operator bool() && get().has_value();
}

std::any TableRowPropertyValue::get() const {
  auto properties = m_impl->row_properties(m_row);
  return internal::util::map::lookup_map_default(properties, m_property,
                                                 std::any());
}

void TableRowPropertyValue::set(const std::any &value) const {
  m_impl->update_row_properties(m_row, {{m_property, value}});
}

void TableRowPropertyValue::remove() const {
  m_impl->update_row_properties(m_row, {{m_property, {}}});
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

TableCellPropertyValue::operator bool() const {
  return m_impl.operator bool() && get().has_value();
}

std::any TableCellPropertyValue::get() const {
  auto properties = m_impl->cell_properties(m_row, m_column);
  return internal::util::map::lookup_map_default(properties, m_property,
                                                 std::any());
}

void TableCellPropertyValue::set(const std::any &value) const {
  m_impl->update_cell_properties(m_row, m_column, {{m_property, value}});
}

void TableCellPropertyValue::remove() const {
  m_impl->update_cell_properties(m_row, m_column, {{m_property, {}}});
}

PropertySet::PropertySet(
    std::unordered_map<ElementProperty, std::any> properties)
    : m_properties{std::move(properties)} {}

std::any PropertySet::get(const ElementProperty property) const {
  auto it = m_properties.find(property);
  if (it == std::end(m_properties)) {
    return {};
  }
  return it->second;
}

std::optional<std::string>
PropertySet::get_string(const ElementProperty property) const {
  return property_value_to_string(get(property));
}

std::optional<std::uint32_t>
PropertySet::get_uint32(const ElementProperty property) const {
  return property_value_to_uint32(get(property));
}

std::optional<bool>
PropertySet::get_bool(const ElementProperty property) const {
  return property_value_to_bool(get(property));
}

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

PropertySet Element::properties() const {
  return PropertySet(m_impl->element_properties(m_id));
}

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

CustomShape Element::custom_shape() const { return CustomShape(m_impl, m_id); }

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
  return property(ElementProperty::NAME).get_string();
}

std::string Slide::notes() const {
  return property(ElementProperty::NOTES).get_string();
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
  return property(ElementProperty::NAME).get_string();
}

Table Sheet::table() const {
  if (!m_impl) {
    return Table();
  }
  return Table(m_impl, m_id);
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
  return property(ElementProperty::NAME).get_string();
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
  return property(ElementProperty::TEXT).get_string();
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
  return property(ElementProperty::HREF).get_string();
}

Bookmark::Bookmark() = default;

Bookmark::Bookmark(std::shared_ptr<const internal::abstract::Document> impl,
                   const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string Bookmark::name() const {
  if (!m_impl) {
    return "";
  }
  return property(ElementProperty::NAME).get_string();
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
    // TODO there is no empty TableDimensions
    return {};
  }
  return m_table->dimensions();
}

TableDimensions Table::content_bounds() const {
  if (!m_impl) {
    // TODO there is no empty TableDimensions
    return {};
  }
  auto content_bounds = m_table->content_bounds();
  return {content_bounds.to().row(), content_bounds.to().column()};
}

TableColumnRange Table::columns() const {
  if (!m_impl) {
    return {};
  }
  return TableColumnRange(TableColumn(m_table, 0),
                          TableColumn(m_table, dimensions().columns));
}

TableRowRange Table::rows() const {
  if (!m_impl) {
    return {};
  }
  return TableRowRange(TableRow(m_table, 0),
                       TableRow(m_table, dimensions().rows));
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

PropertySet TableColumn::properties() const {
  return PropertySet(m_impl->column_properties(m_column));
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

TableCellRange TableRow::cells() const {
  return TableCellRange(first_child(),
                        TableCell(m_impl, m_row, m_impl->dimensions().columns));
}

PropertySet TableRow::properties() const {
  return PropertySet(m_impl->row_properties(m_row));
}

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
  // TODO consider colspan
  return TableCell(m_impl, m_row, m_column - 1);
}

TableCell TableCell::next_sibling() const {
  // TODO performance
  return TableCell(m_impl, m_row, m_column + column_span());
}

ElementRange TableCell::children() const {
  return ElementRange(
      Element(m_impl->document(), m_impl->cell_first_child(m_row, m_column)));
}

PropertySet TableCell::properties() const {
  return PropertySet(m_impl->cell_properties(m_row, m_column));
}

TableCellPropertyValue
TableCell::property(const ElementProperty property) const {
  return TableCellPropertyValue(m_impl, m_row, m_column, property);
}

std::uint32_t TableCell::row_span() const {
  if (!m_impl) {
    return 0;
  }
  return m_impl->cell_span(m_row, m_column).rows;
}

std::uint32_t TableCell::column_span() const {
  if (!m_impl) {
    return 0;
  }
  return m_impl->cell_span(m_row, m_column).columns;
}

Frame::Frame() = default;

Frame::Frame(std::shared_ptr<const internal::abstract::Document> impl,
             const std::uint64_t id)
    : Element(std::move(impl), id) {}

ElementPropertyValue Frame::anchor_type() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::ANCHOR_TYPE);
}

ElementPropertyValue Frame::x() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X);
}

ElementPropertyValue Frame::y() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y);
}

ElementPropertyValue Frame::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue Frame::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
}

ElementPropertyValue Frame::z_index() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Z_INDEX);
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
    const std::string href = this->href().get_string();
    const internal::common::Path path{href};

    return m_impl->files()->is_file(path);
  } catch (...) {
  }

  return false;
}

ElementPropertyValue Image::href() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HREF);
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

  const std::string href = this->href().get_string();
  const internal::common::Path path{href};

  return File(m_impl->files()->open(path));
}

Rect::Rect() = default;

Rect::Rect(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

ElementPropertyValue Rect::x() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X);
}

ElementPropertyValue Rect::y() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y);
}

ElementPropertyValue Rect::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue Rect::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
}

Line::Line() = default;

Line::Line(std::shared_ptr<const internal::abstract::Document> impl,
           const std::uint64_t id)
    : Element(std::move(impl), id) {}

ElementPropertyValue Line::x1() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X1);
}

ElementPropertyValue Line::y1() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y1);
}

ElementPropertyValue Line::x2() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X2);
}

ElementPropertyValue Line::y2() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y2);
}

Circle::Circle() = default;

Circle::Circle(std::shared_ptr<const internal::abstract::Document> impl,
               const std::uint64_t id)
    : Element(std::move(impl), id) {}

ElementPropertyValue Circle::x() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X);
}

ElementPropertyValue Circle::y() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y);
}

ElementPropertyValue Circle::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue Circle::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
}

CustomShape::CustomShape() = default;

CustomShape::CustomShape(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

ElementPropertyValue CustomShape::x() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::X);
}

ElementPropertyValue CustomShape::y() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::Y);
}

ElementPropertyValue CustomShape::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue CustomShape::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
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

} // namespace odr
