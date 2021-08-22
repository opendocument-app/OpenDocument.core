#ifndef ODR_ELEMENT_H
#define ODR_ELEMENT_H

#include <any>
#include <cstdint>
#include <memory>
#include <odr/any_thing.h>
#include <odr/property.h>
#include <odr/table.h>
#include <string>
#include <unordered_map>

namespace odr::internal::abstract {
class Element;
} // namespace odr::internal::abstract

namespace odr::internal::odf {
class OpenDocument;
class Table;
} // namespace odr::internal::odf

namespace odr::internal::ooxml {
class OfficeOpenXmlTextDocument;
class OfficeOpenXmlPresentation;
class OfficeOpenXmlWorkbook;
} // namespace odr::internal::ooxml

namespace odr {

class File;

class Element;
class Slide;
class Sheet;
class Page;
class Text;
class Link;
class Bookmark;
class TableElement;
class Frame;
class Image;
class Rect;
class Line;
class Circle;
class CustomShape;

template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
using SlideRange = ElementRangeTemplate<Slide>;
using SheetRange = ElementRangeTemplate<Sheet>;
using PageRange = ElementRangeTemplate<Page>;

class ElementPropertyValue;
class ElementPropertySet;

class PageStyle;

class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

enum class ElementType {
  NONE,

  ROOT,
  SLIDE,
  SHEET,
  PAGE,

  TEXT,
  LINE_BREAK,
  PAGE_BREAK,
  PARAGRAPH,
  SPAN,
  LINK,
  BOOKMARK,

  LIST,
  LIST_ITEM,

  TABLE,
  TABLE_COLUMN,
  TABLE_ROW,
  TABLE_CELL,

  FRAME,
  IMAGE,
  RECT,
  LINE,
  CIRCLE,
  CUSTOM_SHAPE,

  GROUP,
};

// TODO the property handle could reflect the type
// TODO it might be possible to strongly type these properties
enum class ElementProperty {
  NONE,

  NAME,
  NOTES,
  TEXT,
  HREF,
  ANCHOR_TYPE,

  STYLE_NAME,
  MASTER_PAGE_NAME,

  VALUE_TYPE,

  X,
  Y,
  WIDTH,
  HEIGHT,

  X1,
  Y1,
  X2,
  Y2,

  Z_INDEX,

  TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME,
  TABLE_CELL_BACKGROUND_COLOR,
  ROW_SPAN,
  COLUMN_SPAN,
  ROWS_REPEATED,
  COLUMNS_REPEATED,

  MARGIN_TOP,
  MARGIN_BOTTOM,
  MARGIN_LEFT,
  MARGIN_RIGHT,

  PADDING_TOP,
  PADDING_BOTTOM,
  PADDING_LEFT,
  PADDING_RIGHT,

  BORDER_TOP,
  BORDER_BOTTOM,
  BORDER_LEFT,
  BORDER_RIGHT,

  PRINT_ORIENTATION,

  FONT_NAME,
  FONT_SIZE,
  FONT_WEIGHT,
  FONT_STYLE,
  FONT_UNDERLINE,
  FONT_STRIKETHROUGH,
  FONT_SHADOW,
  FONT_COLOR,

  BACKGROUND_COLOR,

  TEXT_ALIGN,

  STROKE_WIDTH,
  STROKE_COLOR,
  FILL_COLOR,
  VERTICAL_ALIGN,
};

class Element : public AnyThing<internal::abstract::Element> {
public:
  Element();

  template <typename Derived,
            typename = typename std::enable_if<std::is_base_of<
                internal::abstract::Element, Derived>::value>::type>
  explicit Element(Derived derived) : Base{std::move(derived)} {}

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] ElementPropertySet properties() const;
  [[nodiscard]] ElementPropertyValue property(ElementProperty property) const;

  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
  [[nodiscard]] Text text() const;
  [[nodiscard]] Element line_break() const;
  [[nodiscard]] Element page_break() const;
  [[nodiscard]] Element paragraph() const;
  [[nodiscard]] Element span() const;
  [[nodiscard]] Link link() const;
  [[nodiscard]] Bookmark bookmark() const;
  [[nodiscard]] Element list() const;
  [[nodiscard]] Element list_item() const;
  [[nodiscard]] TableElement table() const;
  [[nodiscard]] Element table_column() const;
  [[nodiscard]] Element table_row() const;
  [[nodiscard]] Element table_cell() const;
  [[nodiscard]] Frame frame() const;
  [[nodiscard]] Image image() const;
  [[nodiscard]] Rect rect() const;
  [[nodiscard]] Line line() const;
  [[nodiscard]] Circle circle() const;
  [[nodiscard]] CustomShape custom_shape() const;
  [[nodiscard]] Element group() const;

private:
  friend class Table;
  friend class PageStyle;
  template <typename E> friend class ElementRangeTemplate;
  friend class ElementPropertyValue;
  friend class Document;
};

template <typename E> class ElementIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = E;
  using difference_type = std::ptrdiff_t;
  using pointer = E *;
  using reference = E &;

  explicit ElementIterator(E element) : m_element{std::move(element)} {}

  ElementIterator<E> &operator++() {
    m_element = m_element.next_sibling();
    return *this;
  }

  ElementIterator<E> operator++(int) & {
    ElementIterator<E> result = *this;
    operator++();
    return result;
  }

  reference operator*() { return m_element; }

  pointer operator->() { return &m_element; }

  bool operator==(const ElementIterator<E> &rhs) const {
    return m_element == rhs.m_element;
  }

  bool operator!=(const ElementIterator<E> &rhs) const {
    return m_element != rhs.m_element;
  }

private:
  E m_element;
};

template <typename E> class ElementRangeTemplate final {
public:
  ElementRangeTemplate() = default;

  explicit ElementRangeTemplate(E begin) : m_begin{begin} {}

  ElementRangeTemplate(E begin, E end)
      : m_begin{std::move(begin)}, m_end{std::move(end)} {}

  [[nodiscard]] ElementIterator<E> begin() const {
    return ElementIterator<E>(m_begin);
  }

  [[nodiscard]] ElementIterator<E> end() const {
    return ElementIterator<E>(m_end);
  }

  [[nodiscard]] E front() const { return m_begin; }

private:
  E m_begin;
  E m_end;
};

class Slide final : public Element {
public:
  [[nodiscard]] Slide previous_sibling() const;
  [[nodiscard]] Slide next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::string notes() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  Slide();
  explicit Slide(Element element);

  friend Presentation;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Sheet final : public Element {
public:
  [[nodiscard]] Sheet previous_sibling() const;
  [[nodiscard]] Sheet next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] TableElement table() const;

private:
  Sheet();
  explicit Sheet(Element element);

  friend Spreadsheet;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Page final : public Element {
public:
  [[nodiscard]] Page previous_sibling() const;
  [[nodiscard]] Page next_sibling() const;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  Page();
  explicit Page(Element element);

  friend Drawing;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Text final : public Element {
public:
  [[nodiscard]] std::string string() const;

private:
  Text();
  explicit Text(Element element);

  friend Element;
};

class Link final : public Element {
public:
  [[nodiscard]] std::string href() const;

private:
  Link();
  explicit Link(Element element);

  friend Element;
};

class Bookmark final : public Element {
public:
  [[nodiscard]] std::string name() const;

private:
  Bookmark();
  explicit Bookmark(Element element);

  friend Element;
};

class TableElement final : public Element {
public:
  [[nodiscard]] TableDimensions dimensions() const;
  [[nodiscard]] TableDimensions content_bounds() const;
  [[nodiscard]] TableDimensions content_bounds(TableDimensions within) const;

  [[nodiscard]] Element column(std::uint32_t column) const;
  [[nodiscard]] Element row(std::uint32_t row) const;
  [[nodiscard]] Element cell(std::uint32_t row, std::uint32_t column) const;

private:
  TableElement();
  explicit TableElement(const Element &element);

  Table m_table;

  friend Element;
  friend Sheet;
};

class Frame final : public Element {
public:
  [[nodiscard]] ElementPropertyValue anchor_type() const;
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;
  [[nodiscard]] ElementPropertyValue z_index() const;

private:
  Frame();
  explicit Frame(Element element);

  friend Element;
};

class Image final : public Element {
public:
  [[nodiscard]] bool internal() const;
  [[nodiscard]] ElementPropertyValue href() const;
  [[nodiscard]] File image_file() const;

private:
  Image();
  explicit Image(Element element);

  friend Element;
};

class Rect final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  Rect();
  explicit Rect(Element element);

  friend Element;
};

class Line final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x1() const;
  [[nodiscard]] ElementPropertyValue y1() const;
  [[nodiscard]] ElementPropertyValue x2() const;
  [[nodiscard]] ElementPropertyValue y2() const;

private:
  Line();
  explicit Line(Element element);

  friend Element;
};

class Circle final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  Circle();
  explicit Circle(Element element);

  friend Element;
};

class CustomShape final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  CustomShape();
  explicit CustomShape(Element element);

  friend Element;
};

class ElementPropertyValue final : public PropertyValue {
public:
  bool operator==(const ElementPropertyValue &rhs) const;
  bool operator!=(const ElementPropertyValue &rhs) const;
  explicit operator bool() const final;

  [[nodiscard]] std::any get() const final;

private:
  Element m_element;
  ElementProperty m_property{ElementProperty::NONE};

  ElementPropertyValue();
  ElementPropertyValue(Element element, ElementProperty property);

  friend Element;
  friend Frame;
  friend Image;
  friend Rect;
  friend Line;
  friend Circle;
  friend CustomShape;
  friend PageStyle;
};

class ElementPropertySet {
public:
  [[nodiscard]] std::any get(ElementProperty property) const;
  [[nodiscard]] std::optional<std::string>
  get_string(ElementProperty property) const;
  [[nodiscard]] std::optional<std::uint32_t>
  get_uint32(ElementProperty property) const;
  [[nodiscard]] std::optional<bool> get_bool(ElementProperty property) const;

private:
  explicit ElementPropertySet(
      std::unordered_map<ElementProperty, std::any> properties);

  std::unordered_map<ElementProperty, std::any> m_properties;

  friend Element;
  friend Frame;
  friend Image;
  friend Rect;
  friend Line;
  friend Circle;
  friend CustomShape;
  friend PageStyle;
};

class PageStyle {
public:
  bool operator==(const PageStyle &rhs) const;
  bool operator!=(const PageStyle &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

  [[nodiscard]] ElementPropertyValue margin_top() const;
  [[nodiscard]] ElementPropertyValue margin_bottom() const;
  [[nodiscard]] ElementPropertyValue margin_left() const;
  [[nodiscard]] ElementPropertyValue margin_right() const;

private:
  Element m_element;

  PageStyle();
  explicit PageStyle(Element element);

  friend TextDocument;
  friend Slide;
  friend Page;
};

} // namespace odr

#endif // ODR_ELEMENT_H
