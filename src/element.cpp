#include <internal/abstract/element.h>
#include <internal/common/path.h>
#include <internal/util/map_util.h>
#include <odr/element.h>
#include <odr/file.h>
#include <stdexcept>

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

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

Element::Element() = default;

ElementType Element::type() const {
  if (!operator bool()) {
    return ElementType::NONE;
  }
  return get()->type();
}

Element Element::parent() const {
  if (!operator bool()) {
    return {};
  }
  return get()->parent();
}

Element Element::first_child() const {
  if (!operator bool()) {
    return {};
  }
  return get()->first_child();
}

Element Element::previous_sibling() const {
  if (!operator bool()) {
    return {};
  }
  return get()->previous_sibling();
}

Element Element::next_sibling() const {
  if (!operator bool()) {
    return {};
  }
  return get()->next_sibling();
}

ElementRange Element::children() const { return ElementRange(first_child()); }

ElementPropertySet Element::properties() const {
  return ElementPropertySet(get()->properties());
}

ElementPropertyValue Element::property(const ElementProperty property) const {
  return {*this, property};
}

Slide Element::slide() const {
  if (type() != ElementType::SLIDE) {
    return {};
  }
  return Slide(*this);
}

Sheet Element::sheet() const {
  if (type() != ElementType::SHEET) {
    return {};
  }
  return Sheet(*this);
}

Page Element::page() const {
  if (type() != ElementType::PAGE) {
    return {};
  }
  return Page(*this);
}

Text Element::text() const {
  if (type() != ElementType::TEXT) {
    return {};
  }
  return Text(*this);
}

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

Element Element::paragraph() const {
  if (type() != ElementType::PARAGRAPH) {
    return {};
  }
  return *this;
}

Element Element::span() const {
  if (type() != ElementType::SPAN) {
    return {};
  }
  return *this;
}

Link Element::link() const {
  if (type() != ElementType::LINK) {
    return {};
  }
  return Link(*this);
}

Bookmark Element::bookmark() const {
  if (type() != ElementType::BOOKMARK) {
    return {};
  }
  return Bookmark(*this);
}

Element Element::list() const {
  if (type() != ElementType::LIST) {
    return {};
  }
  return *this;
}

Element Element::list_item() const {
  if (type() != ElementType::LIST_ITEM) {
    return {};
  }
  return *this;
}

TableElement Element::table() const {
  if (type() != ElementType::TABLE) {
    return {};
  }
  return TableElement(*this);
}

Element Element::table_column() const {
  if (type() != ElementType::TABLE_COLUMN) {
    return {};
  }
  return *this;
}

Element Element::table_row() const {
  if (type() != ElementType::TABLE_ROW) {
    return {};
  }
  return *this;
}

Element Element::table_cell() const {
  if (type() != ElementType::TABLE_CELL) {
    return {};
  }
  return *this;
}

Frame Element::frame() const {
  if (type() != ElementType::FRAME) {
    return {};
  }
  return Frame(*this);
}

Image Element::image() const {
  if (type() != ElementType::IMAGE) {
    return {};
  }
  return Image(*this);
}

Rect Element::rect() const {
  if (type() != ElementType::RECT) {
    return {};
  }
  return Rect(*this);
}

Line Element::line() const {
  if (type() != ElementType::LINE) {
    return {};
  }
  return Line(*this);
}

Circle Element::circle() const {
  if (type() != ElementType::CIRCLE) {
    return {};
  }
  return Circle(*this);
}

CustomShape Element::custom_shape() const {
  if (type() != ElementType::CUSTOM_SHAPE) {
    return {};
  }
  return CustomShape(*this);
}

Element Element::group() const {
  if (type() != ElementType::GROUP) {
    return {};
  }
  return *this;
}

Slide::Slide() = default;

Slide::Slide(Element element) : Element{std::move(element)} {}

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

PageStyle Slide::page_style() const { return PageStyle(*this); }

Sheet::Sheet() = default;

Sheet::Sheet(Element element) : Element{std::move(element)} {}

Sheet Sheet::previous_sibling() const {
  return Element::previous_sibling().sheet();
}

Sheet Sheet::next_sibling() const { return Element::next_sibling().sheet(); }

std::string Sheet::name() const {
  return property(ElementProperty::NAME).get_string();
}

TableElement Sheet::table() const {
  if (!operator bool()) {
    return {};
  }
  return first_child().table();
}

Page::Page() = default;

Page::Page(Element element) : Element{std::move(element)} {}

Page Page::previous_sibling() const {
  return Element::previous_sibling().page();
}

Page Page::next_sibling() const { return Element::next_sibling().page(); }

std::string Page::name() const {
  if (!operator bool()) {
    return "";
  }
  return property(ElementProperty::NAME).get_string();
}

PageStyle Page::page_style() const { return PageStyle(*this); }

Text::Text() = default;

Text::Text(Element element) : Element{std::move(element)} {}

std::string Text::string() const {
  if (!operator bool()) {
    return "";
  }
  return property(ElementProperty::TEXT).get_string();
}

Link::Link() = default;

Link::Link(Element element) : Element{std::move(element)} {}

std::string Link::href() const {
  if (!operator bool()) {
    return "";
  }
  return property(ElementProperty::HREF).get_string();
}

Bookmark::Bookmark() = default;

Bookmark::Bookmark(Element element) : Element{std::move(element)} {}

std::string Bookmark::name() const {
  if (!operator bool()) {
    return "";
  }
  return property(ElementProperty::NAME).get_string();
}

TableElement::TableElement() = default;

TableElement::TableElement(const Element &element)
    : Element{element}, m_table{get()->table()} {}

TableDimensions TableElement::dimensions() const {
  if (!operator bool()) {
    // TODO there is no empty TableDimensions
    return {};
  }
  return m_table.dimensions();
}

TableDimensions TableElement::content_bounds() const {
  if (!operator bool()) {
    // TODO there is no empty TableDimensions
    return {};
  }
  return m_table.content_bounds();
}

TableDimensions
TableElement::content_bounds(const TableDimensions within) const {
  if (!operator bool()) {
    // TODO there is no empty TableDimensions
    return {};
  }
  return m_table.content_bounds(within);
}

Element TableElement::column(const std::uint32_t column) const {
  return m_table.column(column);
}

Element TableElement::row(const std::uint32_t row) const {
  return m_table.row(row);
}

Element TableElement::cell(std::uint32_t row, std::uint32_t column) const {
  return m_table.cell(row, column);
}

Frame::Frame() = default;

Frame::Frame(Element element) : Element{std::move(element)} {}

ElementPropertyValue Frame::anchor_type() const {
  return {*this, ElementProperty::ANCHOR_TYPE};
}

ElementPropertyValue Frame::x() const { return {*this, ElementProperty::X}; }

ElementPropertyValue Frame::y() const { return {*this, ElementProperty::Y}; }

ElementPropertyValue Frame::width() const {
  return {*this, ElementProperty::WIDTH};
}

ElementPropertyValue Frame::height() const {
  return {*this, ElementProperty::HEIGHT};
}

ElementPropertyValue Frame::z_index() const {
  return {*this, ElementProperty::Z_INDEX};
}

Image::Image() = default;

Image::Image(Element element) : Element{std::move(element)} {}

bool Image::internal() const {
  // TODO
  return false;
}

ElementPropertyValue Image::href() const {
  return {*this, ElementProperty::HREF};
}

File Image::image_file() const {
  // TODO
  return File(std::shared_ptr<internal::abstract::File>{});
}

Rect::Rect() = default;

Rect::Rect(Element element) : Element{std::move(element)} {}

ElementPropertyValue Rect::x() const { return {*this, ElementProperty::X}; }

ElementPropertyValue Rect::y() const { return {*this, ElementProperty::Y}; }

ElementPropertyValue Rect::width() const {
  return {*this, ElementProperty::WIDTH};
}

ElementPropertyValue Rect::height() const {
  return {*this, ElementProperty::HEIGHT};
}

Line::Line() = default;

Line::Line(Element element) : Element{std::move(element)} {}

ElementPropertyValue Line::x1() const { return {*this, ElementProperty::X1}; }

ElementPropertyValue Line::y1() const { return {*this, ElementProperty::Y1}; }

ElementPropertyValue Line::x2() const { return {*this, ElementProperty::X2}; }

ElementPropertyValue Line::y2() const { return {*this, ElementProperty::Y2}; }

Circle::Circle() = default;

Circle::Circle(Element element) : Element{std::move(element)} {}

ElementPropertyValue Circle::x() const { return {*this, ElementProperty::X}; }

ElementPropertyValue Circle::y() const { return {*this, ElementProperty::Y}; }

ElementPropertyValue Circle::width() const {
  return {*this, ElementProperty::WIDTH};
}

ElementPropertyValue Circle::height() const {
  return {*this, ElementProperty::HEIGHT};
}

CustomShape::CustomShape() = default;

CustomShape::CustomShape(Element element) : Element{std::move(element)} {}

ElementPropertyValue CustomShape::x() const {
  return {*this, ElementProperty::X};
}

ElementPropertyValue CustomShape::y() const {
  return {*this, ElementProperty::Y};
}

ElementPropertyValue CustomShape::width() const {
  return {*this, ElementProperty::WIDTH};
}

ElementPropertyValue CustomShape::height() const {
  return {*this, ElementProperty::HEIGHT};
}

std::string PropertyValue::get_string() const {
  return *property_value_to_string(get());
}

std::uint32_t PropertyValue::get_uint32() const {
  return *property_value_to_uint32(get());
}

bool PropertyValue::get_bool() const { return *property_value_to_bool(get()); }

ElementPropertyValue::ElementPropertyValue() = default;

ElementPropertyValue::ElementPropertyValue(Element element,
                                           const ElementProperty property)
    : m_element{std::move(element)}, m_property{property} {}

bool ElementPropertyValue::operator==(const ElementPropertyValue &rhs) const {
  return m_element == rhs.m_element && m_property == rhs.m_property;
}

bool ElementPropertyValue::operator!=(const ElementPropertyValue &rhs) const {
  return m_element != rhs.m_element || m_property != rhs.m_property;
}

ElementPropertyValue::operator bool() const {
  return m_element && get().has_value();
}

std::any ElementPropertyValue::get() const {
  auto properties = m_element.get()->properties();
  return internal::util::map::lookup_map_default(properties, m_property,
                                                 std::any());
}

ElementPropertySet::ElementPropertySet(
    std::unordered_map<ElementProperty, std::any> properties)
    : m_properties{std::move(properties)} {}

std::any ElementPropertySet::get(const ElementProperty property) const {
  auto it = m_properties.find(property);
  if (it == std::end(m_properties)) {
    return {};
  }
  return it->second;
}

std::optional<std::string>
ElementPropertySet::get_string(const ElementProperty property) const {
  return property_value_to_string(get(property));
}

std::optional<std::uint32_t>
ElementPropertySet::get_uint32(const ElementProperty property) const {
  return property_value_to_uint32(get(property));
}

std::optional<bool>
ElementPropertySet::get_bool(const ElementProperty property) const {
  return property_value_to_bool(get(property));
}

PageStyle::PageStyle() = default;

PageStyle::PageStyle(Element element) : m_element{std::move(element)} {}

bool PageStyle::operator==(const PageStyle &rhs) const {
  return m_element == rhs.m_element;
}

bool PageStyle::operator!=(const PageStyle &rhs) const {
  return m_element != rhs.m_element;
}

PageStyle::operator bool() const { return m_element.operator bool(); }

ElementPropertyValue PageStyle::width() const {
  return {m_element, ElementProperty::WIDTH};
}

ElementPropertyValue PageStyle::height() const {
  return {m_element, ElementProperty::HEIGHT};
}

ElementPropertyValue PageStyle::margin_top() const {
  return {m_element, ElementProperty::MARGIN_TOP};
}

ElementPropertyValue PageStyle::margin_bottom() const {
  return {m_element, ElementProperty::MARGIN_BOTTOM};
}

ElementPropertyValue PageStyle::margin_left() const {
  return {m_element, ElementProperty::MARGIN_LEFT};
}

ElementPropertyValue PageStyle::margin_right() const {
  return {m_element, ElementProperty::MARGIN_RIGHT};
}

} // namespace odr
