#include <internal/abstract/document.h>
#include <internal/abstract/table.h>
#include <internal/common/path.h>
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

SlideElement Element::slide() const { return SlideElement(m_impl, m_id); }

SheetElement Element::sheet() const { return SheetElement(m_impl, m_id); }

PageElement Element::page() const { return PageElement(m_impl, m_id); }

TextElement Element::text() const { return TextElement(m_impl, m_id); }

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

ParagraphElement Element::paragraph() const {
  return ParagraphElement(m_impl, m_id);
}

SpanElement Element::span() const { return SpanElement(m_impl, m_id); }

LinkElement Element::link() const { return LinkElement(m_impl, m_id); }

BookmarkElement Element::bookmark() const {
  return BookmarkElement(m_impl, m_id);
}

ListElement Element::list() const { return ListElement(m_impl, m_id); }

ListItemElement Element::list_item() const {
  return ListItemElement(m_impl, m_id);
}

TableElement Element::table() const { return TableElement(m_impl, m_id); }

FrameElement Element::frame() const { return FrameElement(m_impl, m_id); }

ImageElement Element::image() const { return ImageElement(m_impl, m_id); }

RectElement Element::rect() const { return RectElement(m_impl, m_id); }

LineElement Element::line() const { return LineElement(m_impl, m_id); }

CircleElement Element::circle() const { return CircleElement(m_impl, m_id); }

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
template class ElementIterator<SlideElement>;
template class ElementIterator<SheetElement>;
template class ElementIterator<PageElement>;
template class ElementIterator<TableColumnElement>;
template class ElementIterator<TableRowElement>;
template class ElementIterator<TableCellElement>;

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
template class ElementRangeTemplate<SlideElement>;
template class ElementRangeTemplate<SheetElement>;
template class ElementRangeTemplate<PageElement>;
template class ElementRangeTemplate<TableColumnElement>;
template class ElementRangeTemplate<TableRowElement>;
template class ElementRangeTemplate<TableCellElement>;

SlideElement::SlideElement() = default;

SlideElement::SlideElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SlideElement SlideElement::previous_sibling() const {
  return Element::previous_sibling().slide();
}

SlideElement SlideElement::next_sibling() const {
  return Element::next_sibling().slide();
}

std::string SlideElement::name() const {
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::NAME));
}

std::string SlideElement::notes() const {
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::NOTES));
}

PageStyle SlideElement::page_style() const { return PageStyle(m_impl, m_id); }

SheetElement::SheetElement() = default;

SheetElement::SheetElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SheetElement SheetElement::previous_sibling() const {
  return Element::previous_sibling().sheet();
}

SheetElement SheetElement::next_sibling() const {
  return Element::next_sibling().sheet();
}

std::string SheetElement::name() const {
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::NAME));
}

TableElement SheetElement::table() const {
  if (!m_impl) {
    return TableElement();
  }
  return TableElement(m_impl, m_id); // TODO
}

PageElement::PageElement() = default;

PageElement::PageElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

PageElement PageElement::previous_sibling() const {
  return Element::previous_sibling().page();
}

PageElement PageElement::next_sibling() const {
  return Element::next_sibling().page();
}

std::string PageElement::name() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::NAME));
}

PageStyle PageElement::page_style() const { return PageStyle(m_impl, m_id); }

TextElement::TextElement() = default;

TextElement::TextElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string TextElement::string() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::TEXT));
}

ParagraphElement::ParagraphElement() = default;

ParagraphElement::ParagraphElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

SpanElement::SpanElement() = default;

SpanElement::SpanElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

LinkElement::LinkElement() = default;

LinkElement::LinkElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string LinkElement::href() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::HREF));
}

BookmarkElement::BookmarkElement() = default;

BookmarkElement::BookmarkElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string BookmarkElement::name() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::NAME));
}

ListElement::ListElement() = default;

ListElement::ListElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

ListItemElement::ListItemElement() = default;

ListItemElement::ListItemElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

TableElement::TableElement() = default;

TableElement::TableElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id), m_table{m_impl->table(id)} {}

TableDimensions TableElement::dimensions() const {
  if (!m_impl) {
    return TableDimensions(); // TODO
  }
  // TODO limit?
  return m_table->dimensions(0, 0);
}

TableColumnRange TableElement::columns() const {
  if (!m_impl) {
    return TableColumnRange();
  }
  return TableColumnRange(); // TODO
}

TableRowRange TableElement::rows() const {
  if (!m_impl) {
    return TableRowRange();
  }
  return TableRowRange(); // TODO
}

TableColumnElement::TableColumnElement() = default;

TableColumnElement::TableColumnElement(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t column)
    : m_impl{std::move(impl)}, m_column{column} {}

bool TableColumnElement::operator==(const TableColumnElement &rhs) const {
  return m_impl == rhs.m_impl && m_column == rhs.m_column;
}

bool TableColumnElement::operator!=(const TableColumnElement &rhs) const {
  return m_impl != rhs.m_impl || m_column != rhs.m_column;
}

TableColumnElement::operator bool() const { return m_impl.operator bool(); }

TableColumnElement TableColumnElement::previous_sibling() const {
  return TableColumnElement(m_impl, m_column - 1);
}

TableColumnElement TableColumnElement::next_sibling() const {
  return TableColumnElement(m_impl, m_column + 1);
}

TableColumnPropertyValue
TableColumnElement::property(const ElementProperty property) const {
  return TableColumnPropertyValue(m_impl, m_column, property);
}

TableRowElement::TableRowElement() = default;

TableRowElement::TableRowElement(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t row)
    : m_impl{std::move(impl)}, m_row{row} {}

bool TableRowElement::operator==(const TableRowElement &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row;
}

bool TableRowElement::operator!=(const TableRowElement &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row;
}

TableRowElement::operator bool() const { return m_impl.operator bool(); }

TableCellElement TableRowElement::first_child() const {
  if (!m_impl) {
    return TableCellElement();
  }
  return TableCellElement(m_impl, m_row, 0);
}

TableRowElement TableRowElement::previous_sibling() const {
  return TableRowElement(m_impl, m_row - 1);
}

TableRowElement TableRowElement::next_sibling() const {
  return TableRowElement(m_impl, m_row + 1);
}

TableCellRange TableRowElement::cells() const {
  return TableCellRange(first_child());
}

TableRowPropertyValue
TableRowElement::property(const ElementProperty property) const {
  return TableRowPropertyValue(m_impl, m_row, property);
}

TableCellElement::TableCellElement() = default;

TableCellElement::TableCellElement(
    std::shared_ptr<const internal::abstract::Table> impl,
    const std::uint32_t row, const std::uint32_t column)
    : m_impl{std::move(impl)}, m_row{row}, m_column{column} {}

bool TableCellElement::operator==(const TableCellElement &rhs) const {
  return m_impl == rhs.m_impl && m_row == rhs.m_row && m_column == rhs.m_column;
}

bool TableCellElement::operator!=(const TableCellElement &rhs) const {
  return m_impl != rhs.m_impl || m_row != rhs.m_row || m_column != rhs.m_column;
}

TableCellElement::operator bool() const { return m_impl.operator bool(); }

TableCellElement TableCellElement::previous_sibling() const {
  return TableCellElement(m_impl, m_row, m_column - 1);
}

TableCellElement TableCellElement::next_sibling() const {
  return TableCellElement(m_impl, m_row, m_column + 1);
}

ElementRange TableCellElement::children() const {
  return ElementRange(
      Element(nullptr, m_impl->cell_first_child(m_row, m_column)));
}

TableCellPropertyValue
TableCellElement::property(const ElementProperty property) const {
  return TableCellPropertyValue(m_impl, m_row, m_column, property);
}

std::uint32_t TableCellElement::row_span() const {
  if (!m_impl) {
    return 0;
  }
  return property_value_to_uint32(m_impl->cell_property(
      m_row, m_column, ElementProperty::TABLE_CELL_ROW_SPAN));
}

std::uint32_t TableCellElement::column_span() const {
  if (!m_impl) {
    return 0;
  }
  return property_value_to_uint32(m_impl->cell_property(
      m_row, m_column, ElementProperty::TABLE_CELL_COLUMN_SPAN));
}

FrameElement::FrameElement() = default;

FrameElement::FrameElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string FrameElement::anchor_type() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::ANCHOR_TYPE));
}

std::string FrameElement::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::WIDTH));
}

std::string FrameElement::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::HEIGHT));
}

std::string FrameElement::z_index() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::Z_INDEX));
}

ImageElement::ImageElement() = default;

ImageElement::ImageElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

bool ImageElement::internal() const {
  if (!m_impl) {
    return false;
  }
  return property_value_to_bool(
      m_impl->element_property(m_id, ElementProperty::IMAGE_INTERNAL));
}

std::string ImageElement::href() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::HREF));
}

File ImageElement::image_file() const {
  if (!m_impl) {
    // TODO there is no "empty" file
    return File(std::shared_ptr<internal::abstract::File>());
  }
  return File(std::any_cast<std::shared_ptr<internal::abstract::File>>(
      m_impl->element_property(m_id, ElementProperty::IMAGE_FILE)));
}

RectElement::RectElement() = default;

RectElement::RectElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string RectElement::x() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::X));
}

std::string RectElement::y() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::Y));
}

std::string RectElement::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::WIDTH));
}

std::string RectElement::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::HEIGHT));
}

LineElement::LineElement() = default;

LineElement::LineElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string LineElement::x1() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::X1));
}

std::string LineElement::y1() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::Y1));
}

std::string LineElement::x2() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::X2));
}

std::string LineElement::y2() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::Y2));
}

CircleElement::CircleElement() = default;

CircleElement::CircleElement(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id)
    : Element(std::move(impl), id) {}

std::string CircleElement::x() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::X));
}

std::string CircleElement::y() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::Y));
}

std::string CircleElement::width() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::WIDTH));
}

std::string CircleElement::height() const {
  if (!m_impl) {
    return "";
  }
  return property_value_to_string(
      m_impl->element_property(m_id, ElementProperty::HEIGHT));
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
  return SlideRange(SlideElement(m_impl, m_impl->first_entry_element()));
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
  return SheetRange(SheetElement(m_impl, m_impl->first_entry_element()));
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
  return PageRange(PageElement(m_impl, m_impl->first_entry_element()));
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
  return m_impl.operator bool() && m_id != 0;
}

std::any ElementPropertyValue::get() const {
  return m_impl->element_property(m_id, m_property);
}

void ElementPropertyValue::set(const std::any &value) const {
  m_impl->set_element_property(m_id, m_property, value);
}

void ElementPropertyValue::remove() const {
  m_impl->set_element_property(m_id, m_property, {});
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
  return m_impl->column_property(m_column, m_property);
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
  return m_impl->row_property(m_row, m_property);
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
  return m_impl->cell_property(m_row, m_column, m_property);
}

void TableCellPropertyValue::set(const std::any &value) const {
  throw UnsupportedOperation(); // TODO
}

void TableCellPropertyValue::remove() const {
  throw UnsupportedOperation(); // TODO
}

} // namespace odr
