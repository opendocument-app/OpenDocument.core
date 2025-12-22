#pragma once

#include <memory>
#include <optional>
#include <string>

#include <odr/document_element_identifier.hpp>

namespace odr::internal::abstract {
class Document;

class ElementAdapter;
class TextRootAdapter;
class SlideAdapter;
class PageAdapter;
class SheetAdapter;
class SheetColumnAdapter;
class SheetRowAdapter;
class SheetCellAdapter;
class MasterPageAdapter;
class LineBreakAdapter;
class ParagraphAdapter;
class SpanAdapter;
class TextAdapter;
class LinkAdapter;
class BookmarkAdapter;
class ListItemAdapter;
class TableAdapter;
class TableColumnAdapter;
class TableRowAdapter;
class TableCellAdapter;
class FrameAdapter;
class RectAdapter;
class LineAdapter;
class CircleAdapter;
class CustomShapeAdapter;
class ImageAdapter;
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

  sheet_column,
  sheet_row,
  sheet_cell,

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
  Element(const internal::abstract::ElementAdapter *adapter,
          ExtendedElementIdentifier identifier);

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] bool is_editable() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] TextRoot as_text_root() const;
  [[nodiscard]] Slide as_slide() const;
  [[nodiscard]] Sheet as_sheet() const;
  [[nodiscard]] Page as_page() const;
  [[nodiscard]] MasterPage as_master_page() const;
  [[nodiscard]] LineBreak as_line_break() const;
  [[nodiscard]] Paragraph as_paragraph() const;
  [[nodiscard]] Span as_span() const;
  [[nodiscard]] Text as_text() const;
  [[nodiscard]] Link as_link() const;
  [[nodiscard]] Bookmark as_bookmark() const;
  [[nodiscard]] ListItem as_list_item() const;
  [[nodiscard]] Table as_table() const;
  [[nodiscard]] TableColumn as_table_column() const;
  [[nodiscard]] TableRow as_table_row() const;
  [[nodiscard]] TableCell as_table_cell() const;
  [[nodiscard]] Frame as_frame() const;
  [[nodiscard]] Rect as_rect() const;
  [[nodiscard]] Line as_line() const;
  [[nodiscard]] Circle as_circle() const;
  [[nodiscard]] CustomShape as_custom_shape() const;
  [[nodiscard]] Image as_image() const;

protected:
  const internal::abstract::ElementAdapter *m_adapter{nullptr};
  ExtendedElementIdentifier m_identifier;

  friend bool operator==(const Element &lhs, const Element &rhs) {
    return lhs.m_identifier == rhs.m_identifier;
  }

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
  ElementIterator(const internal::abstract::ElementAdapter *adapter,
                  ExtendedElementIdentifier identifier);

  reference operator*() const;

  ElementIterator &operator++();
  ElementIterator operator++(int);

private:
  const internal::abstract::ElementAdapter *m_adapter{nullptr};
  ExtendedElementIdentifier m_identifier;

  friend bool operator==(const ElementIterator &lhs,
                         const ElementIterator &rhs) {
    return lhs.m_identifier == rhs.m_identifier;
  }

  [[nodiscard]] bool exists_() const;
};

/// @brief Represents a range of elements in a document.
class ElementRange {
public:
  ElementRange();
  explicit ElementRange(const ElementIterator &begin);
  ElementRange(const ElementIterator &begin, const ElementIterator &end);

  [[nodiscard]] ElementIterator begin() const;
  [[nodiscard]] ElementIterator end() const;

private:
  ElementIterator m_begin;
  ElementIterator m_end;
};

/// @brief Represents a typed element in a document.
template <typename T> class ElementBase : public Element {
public:
  ElementBase() = default;
  ElementBase(const internal::abstract::ElementAdapter *adapter,
              const ExtendedElementIdentifier identifier, const T *adapter2)
      : Element(adapter, identifier), m_adapter2{adapter2} {}

  explicit operator bool() const { return exists_(); }

protected:
  const T *m_adapter2{nullptr};

  [[nodiscard]] bool exists_() const {
    return Element::exists_() && m_adapter2 != nullptr;
  }
};

/// @brief Represents a root element in a document.
class TextRoot final : public ElementBase<internal::abstract::TextRootAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage first_master_page() const;
};

/// @brief Represents a slide element in a document.
class Slide final : public ElementBase<internal::abstract::SlideAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage master_page() const;
};

/// @brief Represents a sheet element in a document.
class Sheet final : public ElementBase<internal::abstract::SheetAdapter> {
public:
  using ElementBase::ElementBase;

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
class SheetColumn final
    : public ElementBase<internal::abstract::SheetColumnAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TableColumnStyle style() const;
};

/// @brief Represents a sheet row element in a document.
class SheetRow final : public ElementBase<internal::abstract::SheetRowAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TableRowStyle style() const;
};

/// @brief Represents a sheet cell element in a document.
class SheetCell final
    : public ElementBase<internal::abstract::SheetCellAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::uint32_t column() const;
  [[nodiscard]] std::uint32_t row() const;
  [[nodiscard]] bool is_covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] TableCellStyle style() const;
};

/// @brief Represents a page element in a document.
class Page final : public ElementBase<internal::abstract::PageAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] MasterPage master_page() const;
};

/// @brief Represents a master page element in a document.
class MasterPage final
    : public ElementBase<internal::abstract::MasterPageAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] PageLayout page_layout() const;
};

/// @brief Represents a line break element in a document.
class LineBreak final
    : public ElementBase<internal::abstract::LineBreakAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a paragraph element in a document.
class Paragraph final
    : public ElementBase<internal::abstract::ParagraphAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] ParagraphStyle style() const;
  [[nodiscard]] TextStyle text_style() const;
};

/// @brief Represents a span element in a document.
class Span final : public ElementBase<internal::abstract::SpanAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a text element in a document.
class Text final : public ElementBase<internal::abstract::TextAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string content() const;
  void set_content(const std::string &text) const;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a link element in a document.
class Link final : public ElementBase<internal::abstract::LinkAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string href() const;
};

/// @brief Represents a bookmark element in a document.
class Bookmark final : public ElementBase<internal::abstract::BookmarkAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string name() const;
};

/// @brief Represents a list item element in a document.
class ListItem final : public ElementBase<internal::abstract::ListItemAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TextStyle style() const;
};

/// @brief Represents a table element in a document.
class Table final : public ElementBase<internal::abstract::TableAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TableRow first_row() const;
  [[nodiscard]] TableColumn first_column() const;

  [[nodiscard]] ElementRange columns() const;
  [[nodiscard]] ElementRange rows() const;

  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] TableStyle style() const;
};

/// @brief Represents a table column element in a document.
class TableColumn final
    : public ElementBase<internal::abstract::TableColumnAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TableColumnStyle style() const;
};

/// @brief Represents a table row element in a document.
class TableRow final : public ElementBase<internal::abstract::TableRowAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TableRowStyle style() const;
};

/// @brief Represents a table cell element in a document.
class TableCell final
    : public ElementBase<internal::abstract::TableCellAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] bool is_covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] TableCellStyle style() const;
};

/// @brief Represents a frame element in a document.
class Frame final : public ElementBase<internal::abstract::FrameAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] AnchorType anchor_type() const;
  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::optional<std::string> width() const;
  [[nodiscard]] std::optional<std::string> height() const;
  [[nodiscard]] std::optional<std::string> z_index() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a rectangle element in a document.
class Rect final : public ElementBase<internal::abstract::RectAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a line element in a document.
class Line final : public ElementBase<internal::abstract::LineAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a circle element in a document.
class Circle final : public ElementBase<internal::abstract::CircleAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents a custom shape element in a document.
class CustomShape final
    : public ElementBase<internal::abstract::CustomShapeAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

/// @brief Represents an image element in a document.
class Image final : public ElementBase<internal::abstract::ImageAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] bool is_internal() const;
  [[nodiscard]] std::optional<File> file() const;
  [[nodiscard]] std::string href() const;
};

} // namespace odr
