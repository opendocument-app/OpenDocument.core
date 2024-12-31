#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace odr::internal::abstract {
class Document;

class Element;
class TextRoot;
class Slide;
class Sheet;
class SheetCell;
class Page;
class MasterPage;
class LineBreak;
class Paragraph;
class Span;
class Text;
class Link;
class Bookmark;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
class Frame;
class Rect;
class Line;
class Circle;
class CustomShape;
class Image;
} // namespace odr::internal::abstract

namespace odr {

class File;
struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct PageLayout;
struct TableDimensions;

class Element;
class ElementIterator;
class ElementRange;
class TextRoot;
class Slide;
class Sheet;
class SheetColumn;
class SheetRow;
class SheetCell;
class Page;
class MasterPage;
class LineBreak;
class Paragraph;
class Span;
class Text;
class Link;
class Bookmark;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
class Frame;
class Rect;
class Line;
class Circle;
class CustomShape;
class Image;

/// @brief Collection of element types.
enum class ElementType {
  none,

  root,
  slide,
  sheet,
  page,

  master_page,

  text,
  line_break,
  page_break,
  paragraph,
  span,
  link,
  bookmark,

  list,
  list_item,

  table,
  table_column,
  table_row,
  table_cell,

  frame,
  image,
  rect,
  line,
  circle,
  custom_shape,

  group,
};

/// @brief Collection of anchor types.
enum class AnchorType {
  as_char,
  at_char,
  at_frame,
  at_page,
  at_paragraph,
};

/// @brief Collection of value types.
enum class ValueType {
  unknown,
  string,
  float_number,
};

/// @brief Represents an element in a document.
class Element {
public:
  Element();
  Element(const internal::abstract::Document *, internal::abstract::Element *);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] bool is_editable() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] TextRoot text_root() const;
  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
  [[nodiscard]] MasterPage master_page() const;
  [[nodiscard]] LineBreak line_break() const;
  [[nodiscard]] Paragraph paragraph() const;
  [[nodiscard]] Span span() const;
  [[nodiscard]] Text text() const;
  [[nodiscard]] Link link() const;
  [[nodiscard]] Bookmark bookmark() const;
  [[nodiscard]] ListItem list_item() const;
  [[nodiscard]] Table table() const;
  [[nodiscard]] TableColumn table_column() const;
  [[nodiscard]] TableRow table_row() const;
  [[nodiscard]] TableCell table_cell() const;
  [[nodiscard]] Frame frame() const;
  [[nodiscard]] Rect rect() const;
  [[nodiscard]] Line line() const;
  [[nodiscard]] Circle circle() const;
  [[nodiscard]] CustomShape custom_shape() const;
  [[nodiscard]] Image image() const;

protected:
  const internal::abstract::Document *m_document{nullptr};
  internal::abstract::Element *m_element{nullptr};

  [[nodiscard]] bool exists_() const;
};

/// @brief Represents an iterator for elements in a document.
class ElementIterator {
public:
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using pointer = Element *;
  using reference = Element;
  using iterator_category = std::forward_iterator_tag;

  ElementIterator();
  ElementIterator(const internal::abstract::Document *,
                  internal::abstract::Element *);

  bool operator==(const ElementIterator &rhs) const;
  bool operator!=(const ElementIterator &rhs) const;

  reference operator*() const;

  ElementIterator &operator++();
  ElementIterator operator++(int);

private:
  const internal::abstract::Document *m_document{nullptr};
  internal::abstract::Element *m_element{nullptr};

  [[nodiscard]] bool exists_() const;
};

/// @brief Represents a range of elements in a document.
class ElementRange {
public:
  ElementRange();
  explicit ElementRange(ElementIterator begin);
  ElementRange(ElementIterator begin, ElementIterator end);

  [[nodiscard]] ElementIterator begin() const;
  [[nodiscard]] ElementIterator end() const;

private:
  ElementIterator m_begin;
  ElementIterator m_end;
};

/// @brief Represents a typed element in a document.
template <typename T> class TypedElement : public Element {
public:
  TypedElement() = default;
  TypedElement(const internal::abstract::Document *document, T *element)
      : Element(document, element), m_element{element} {}
  TypedElement(const internal::abstract::Document *document,
               internal::abstract::Element *element)
      : Element(document, element), m_element{dynamic_cast<T *>(element)} {}

  bool operator==(const TypedElement &rhs) const {
    return m_element == rhs.m_element;
  }
  bool operator!=(const TypedElement &rhs) const {
    return m_element != rhs.m_element;
  }

protected:
  T *m_element;

  [[nodiscard]] bool exists_() const { return m_element != nullptr; }
};

/// @brief Represents a root element in a document.
class TextRoot final : public TypedElement<internal::abstract::TextRoot> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage first_master_page() const;
};

/// @brief Represents a slide element in a document.
class Slide final : public TypedElement<internal::abstract::Slide> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage master_page() const;
};

/// @brief Represents a sheet element in a document.
class Sheet final : public TypedElement<internal::abstract::Sheet> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] TableDimensions dimensions() const;
  [[nodiscard]] TableDimensions
  content(std::optional<TableDimensions> range) const;

  [[nodiscard]] SheetColumn column(std::uint32_t column) const;
  [[nodiscard]] SheetRow row(std::uint32_t row) const;
  [[nodiscard]] SheetCell cell(std::uint32_t column, std::uint32_t row) const;

  [[nodiscard]] ElementRange shapes() const;
};

/// @brief Represents a sheet column element in a document.
class SheetColumn final : public TypedElement<internal::abstract::Sheet> {
public:
  SheetColumn() = default;
  SheetColumn(const internal::abstract::Document *document,
              internal::abstract::Sheet *sheet, std::uint32_t column);

  [[nodiscard]] TableColumnStyle style() const;

private:
  std::uint32_t m_column{};
};

/// @brief Represents a sheet row element in a document.
class SheetRow final : public TypedElement<internal::abstract::Sheet> {
public:
  SheetRow() = default;
  SheetRow(const internal::abstract::Document *document,
           internal::abstract::Sheet *sheet, std::uint32_t row);

  [[nodiscard]] TableRowStyle style() const;

private:
  std::uint32_t m_row{};
};

/// @brief Represents a sheet cell element in a document.
class SheetCell final : public TypedElement<internal::abstract::SheetCell> {
public:
  SheetCell() = default;
  SheetCell(const internal::abstract::Document *document,
            internal::abstract::Sheet *sheet, std::uint32_t column,
            std::uint32_t row, internal::abstract::SheetCell *element);

  [[nodiscard]] bool is_covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] TableCellStyle style() const;

private:
  internal::abstract::Sheet *m_sheet{};
  std::uint32_t m_column{};
  std::uint32_t m_row{};
};

/// @brief Represents a page element in a document.
class Page final : public TypedElement<internal::abstract::Page> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage master_page() const;
};

/// @brief Represents a master page element in a document.
class MasterPage final : public TypedElement<internal::abstract::MasterPage> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] PageLayout page_layout() const;
};

/// @brief Represents a line break element in a document.
class LineBreak final : public TypedElement<internal::abstract::LineBreak> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a paragraph element in a document.
class Paragraph final : public TypedElement<internal::abstract::Paragraph> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] ParagraphStyle style() const;
  [[nodiscard]] TextStyle text_style() const;
};

/// @brief Represents a span element in a document.
class Span final : public TypedElement<internal::abstract::Span> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a text element in a document.
class Text final : public TypedElement<internal::abstract::Text> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string content() const;
  void set_content(const std::string &text) const;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a link element in a document.
class Link final : public TypedElement<internal::abstract::Link> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string href() const;
};

/// @brief Represents a bookmark element in a document.
class Bookmark final : public TypedElement<internal::abstract::Bookmark> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;
};

/// @brief Represents a list item element in a document.
class ListItem final : public TypedElement<internal::abstract::ListItem> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a table element in a document.
class Table final : public TypedElement<internal::abstract::Table> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] ElementRange columns() const;
  [[nodiscard]] ElementRange rows() const;

  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] TableStyle style() const;
};

/// @brief Represents a table column element in a document.
class TableColumn final : public TypedElement<internal::abstract::TableColumn> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableColumnStyle style() const;
};

/// @brief Represents a table row element in a document.
class TableRow final : public TypedElement<internal::abstract::TableRow> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableRowStyle style() const;
};

/// @brief Represents a table cell element in a document.
class TableCell final : public TypedElement<internal::abstract::TableCell> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] bool is_covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] TableCellStyle style() const;
};

/// @brief Represents a frame element in a document.
class Frame final : public TypedElement<internal::abstract::Frame> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] AnchorType anchor_type() const;
  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::optional<std::string> width() const;
  [[nodiscard]] std::optional<std::string> height() const;
  [[nodiscard]] std::optional<std::string> z_index() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a rectangle element in a document.
class Rect final : public TypedElement<internal::abstract::Rect> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a line element in a document.
class Line final : public TypedElement<internal::abstract::Line> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a circle element in a document.
class Circle final : public TypedElement<internal::abstract::Circle> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a custom shape element in a document.
class CustomShape final : public TypedElement<internal::abstract::CustomShape> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents an image element in a document.
class Image final : public TypedElement<internal::abstract::Image> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] bool is_internal() const;
  [[nodiscard]] std::optional<odr::File> file() const;
  [[nodiscard]] std::string href() const;
};

} // namespace odr
