#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include <odr/definitions.hpp>
#include <odr/quantity.hpp>

namespace odr {
struct TablePosition;
struct TableDimensions;
class DocumentPath;
class File;
struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct PageLayout;
} // namespace odr

namespace odr::internal::abstract {
class Document;

class ElementAdapter;
class TextRootAdapter;
class SlideAdapter;
class PageAdapter;
class SheetAdapter;
class SheetCellAdapter;
class MasterPageAdapter;
class LineBreakAdapter;
class ParagraphAdapter;
class SpanAdapter;
class TextAdapter;
class LinkAdapter;
class BookmarkAdapter;
class ListAdapter;
class ListItemAdapter;
class TableAdapter;
class TableColumnAdapter;
class TableRowAdapter;
class TableCellAdapter;
class FrameAdapter;
class ImageAdapter;
} // namespace odr::internal::abstract

namespace odr {

class Element;
class ElementIterator;
class ElementRange;
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
class List;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
class Frame;
class Image;

/// @brief Collection of element types.
enum class ElementType {
  none,

  root,
  slide,
  sheet,
  page,

  master_page,

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

  group,
};

/// @brief Collection of shapes a frame draws.
enum class ShapeType {
  none, ///< a plain box, drawing no outline of its own
  rect,
  ellipse,
  line,
  custom, ///< an outline of its own, read from @ref Frame::path
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

/// @brief Collection of list types.
enum class ListType {
  unordered,
  ordered,
};

/// @brief Represents an element in a document.
class Element {
public:
  Element();
  Element(const internal::abstract::ElementAdapter *adapter,
          ElementIdentifier identifier);

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] bool is_unique() const;
  [[nodiscard]] bool is_self_locatable() const;
  [[nodiscard]] bool is_editable() const;
  [[nodiscard]] DocumentPath document_path() const;
  [[nodiscard]] Element navigate_path(const DocumentPath &path) const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] TextRoot as_text_root() const;
  [[nodiscard]] Slide as_slide() const;
  [[nodiscard]] Sheet as_sheet() const;
  [[nodiscard]] Page as_page() const;
  [[nodiscard]] SheetCell as_sheet_cell() const;
  [[nodiscard]] MasterPage as_master_page() const;
  [[nodiscard]] LineBreak as_line_break() const;
  [[nodiscard]] Paragraph as_paragraph() const;
  [[nodiscard]] Span as_span() const;
  [[nodiscard]] Text as_text() const;
  [[nodiscard]] Link as_link() const;
  [[nodiscard]] Bookmark as_bookmark() const;
  [[nodiscard]] List as_list() const;
  [[nodiscard]] ListItem as_list_item() const;
  [[nodiscard]] Table as_table() const;
  [[nodiscard]] TableColumn as_table_column() const;
  [[nodiscard]] TableRow as_table_row() const;
  [[nodiscard]] TableCell as_table_cell() const;
  [[nodiscard]] Frame as_frame() const;
  [[nodiscard]] Image as_image() const;

protected:
  const internal::abstract::ElementAdapter *m_adapter{nullptr};
  ElementIdentifier m_identifier{null_element_id};

  /// Identifiers are only unique within one document, so the adapter has to
  /// take part. Elements that don't exist carry no adapter to compare and are
  /// all equal to each other.
  friend bool operator==(const Element &lhs, const Element &rhs) {
    if (!lhs.exists_() || !rhs.exists_()) {
      return lhs.exists_() == rhs.exists_();
    }
    return lhs.m_adapter == rhs.m_adapter &&
           lhs.m_identifier == rhs.m_identifier;
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
                  ElementIdentifier identifier);

  Element operator*() const;

  ElementIterator &operator++();
  ElementIterator operator++(int);

private:
  const internal::abstract::ElementAdapter *m_adapter{nullptr};
  ElementIdentifier m_identifier{null_element_id};

  /// Same rule as @ref Element: compare the adapter too, and treat every
  /// exhausted iterator as the end iterator regardless of which range it came
  /// from.
  friend bool operator==(const ElementIterator &lhs,
                         const ElementIterator &rhs) {
    if (!lhs.exists_() || !rhs.exists_()) {
      return lhs.exists_() == rhs.exists_();
    }
    return lhs.m_adapter == rhs.m_adapter &&
           lhs.m_identifier == rhs.m_identifier;
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
              ElementIdentifier identifier, const T *adapter2)
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

  /// The paper the sheet is meant to be printed on, where the file states one.
  [[nodiscard]] PageLayout page_layout() const;

  [[nodiscard]] TableDimensions dimensions() const;
  [[nodiscard]] TableDimensions
  content(std::optional<TableDimensions> range) const;

  [[nodiscard]] SheetCell cell(std::uint32_t column, std::uint32_t row) const;
  [[nodiscard]] ElementRange shapes() const;

  [[nodiscard]] TableStyle style() const;
  [[nodiscard]] TableColumnStyle column_style(std::uint32_t column) const;
  [[nodiscard]] TableRowStyle row_style(std::uint32_t row) const;
  [[nodiscard]] TableCellStyle cell_style(std::uint32_t column,
                                          std::uint32_t row) const;
};

/// @brief Represents a sheet cell element in a document.
class SheetCell final
    : public ElementBase<internal::abstract::SheetCellAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TablePosition position() const;
  [[nodiscard]] bool is_covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;
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

/// @brief Represents a list element in a document.
class List final : public ElementBase<internal::abstract::ListAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] ListType type() const;
};

/// @brief Represents a list item element in a document.
class ListItem final : public ElementBase<internal::abstract::ListItemAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] TextStyle style() const;

  /// The label this item is marked with, already resolved against the list
  /// style and the running counters — a bullet, "1.", "I.a)", or empty where
  /// the list style asks for no label.
  [[nodiscard]] std::string marker() const;
  /// The counter behind `marker`, absent for an unordered item.
  [[nodiscard]] std::optional<std::uint32_t> number() const;
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

/// @brief Represents a drawing shape's outline, as an svg path.
///
/// `data` is written in the user-space box `x`, `y`, `width`, `height`, which
/// the shape's own box stretches to, aspect ratio not preserved.
struct DrawingPath final {
  std::string data;
  double x{0};
  double y{0};
  double width{0};
  double height{0};
};

/// @brief Represents the two ends of a line shape, in the parent's space.
struct DrawingLine final {
  Measure x1{0, DynamicUnit()};
  Measure y1{0, DynamicUnit()};
  Measure x2{0, DynamicUnit()};
  Measure y2{0, DynamicUnit()};
};

/// @brief Represents the affine transform a drawing shape carries.
///
/// `(x, y)` maps to `(a*x + c*y + e, b*x + d*y + f)`, the lettering of
/// `matrix(a b c d e f)`. `e` and `f` are lengths in the unit the document
/// wrote, and unitless only where they are zero.
struct DrawingTransform final {
  double a{1};
  double b{0};
  double c{0};
  double d{1};
  Measure e{0, DynamicUnit()};
  Measure f{0, DynamicUnit()};
};

/// @brief Represents a frame element in a document.
class Frame final : public ElementBase<internal::abstract::FrameAdapter> {
public:
  using ElementBase::ElementBase;

  [[nodiscard]] ShapeType shape_type() const;
  [[nodiscard]] AnchorType anchor_type() const;
  [[nodiscard]] std::optional<Measure> x() const;
  [[nodiscard]] std::optional<Measure> y() const;
  [[nodiscard]] std::optional<Measure> width() const;
  [[nodiscard]] std::optional<Measure> height() const;
  [[nodiscard]] std::optional<std::int32_t> z_index() const;
  [[nodiscard]] std::optional<DrawingTransform> transform() const;
  /// Nothing for a shape whose geometry we cannot read, leaving its box.
  [[nodiscard]] std::optional<DrawingPath> path() const;
  /// The ends of a @ref ShapeType::line, which states them instead of a box.
  [[nodiscard]] std::optional<DrawingLine> line() const;

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
