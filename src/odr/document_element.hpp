#ifndef ODR_DOCUMENT_ELEMENT_H
#define ODR_DOCUMENT_ELEMENT_H

#include <memory>
#include <optional>
#include <string>

namespace odr::internal::abstract {
class Document;
class DocumentCursor;

class Element;
class TextRootElement;
class SlideElement;
class SheetElement;
class PageElement;
class LineBreakElement;
class ParagraphElement;
class SpanElement;
class TextElement;
class LinkElement;
class BookmarkElement;
class ListItemElement;
class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;
class FrameElement;
class RectElement;
class LineElement;
class CircleElement;
class CustomShapeElement;
class ImageElement;
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
class TextRoot;
class Slide;
class Sheet;
class Page;
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

enum class AnchorType {
  as_char,
  at_char,
  at_frame,
  at_page,
  at_paragraph,
};

enum class ValueType {
  string,
  float_number,
};

class Element {
public:
  Element() = default;
  Element(const internal::abstract::Document *document,
          const internal::abstract::DocumentCursor *cursor,
          internal::abstract::Element *element);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TextRoot text_root() const;
  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
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
  const internal::abstract::DocumentCursor *m_cursor{nullptr};
  internal::abstract::Element *m_element{nullptr};
};

template <typename T> class TypedElement : public Element {
public:
  TypedElement() = default;
  TypedElement(const internal::abstract::Document *document,
               const internal::abstract::DocumentCursor *cursor, T *element)
      : Element(document, cursor, element), m_element{element} {}
  TypedElement(const internal::abstract::Document *document,
               const internal::abstract::DocumentCursor *cursor,
               internal::abstract::Element *element)
      : Element(document, cursor, element), m_element{
                                                dynamic_cast<T *>(element)} {}

protected:
  T *m_element{nullptr};
};

class TextRoot final
    : public TypedElement<internal::abstract::TextRootElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] PageLayout page_layout() const;
};

class Slide final : public TypedElement<internal::abstract::SlideElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;
};

class Sheet final : public TypedElement<internal::abstract::SheetElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] TableDimensions
  content(std::optional<TableDimensions> range) const;
};

class Page final : public TypedElement<internal::abstract::PageElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;
};

class LineBreak final
    : public TypedElement<internal::abstract::LineBreakElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

class Paragraph final
    : public TypedElement<internal::abstract::ParagraphElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] ParagraphStyle style() const;
  [[nodiscard]] TextStyle text_style() const;
};

class Span final : public TypedElement<internal::abstract::SpanElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

class Text final : public TypedElement<internal::abstract::TextElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string content() const;
  void set_content(const std::string &text) const;

  [[nodiscard]] TextStyle style() const;
};

class Link final : public TypedElement<internal::abstract::LinkElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string href() const;
};

class Bookmark final
    : public TypedElement<internal::abstract::BookmarkElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;
};

class ListItem final
    : public TypedElement<internal::abstract::ListItemElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TextStyle style() const;
};

class Table final : public TypedElement<internal::abstract::TableElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] TableStyle style() const;
};

class TableColumn final
    : public TypedElement<internal::abstract::TableColumnElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableColumnStyle style() const;
};

class TableRow final
    : public TypedElement<internal::abstract::TableRowElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableRowStyle style() const;
};

class TableCell final
    : public TypedElement<internal::abstract::TableCellElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableColumn column() const;
  [[nodiscard]] TableRow row() const;

  [[nodiscard]] bool covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] TableCellStyle style() const;
};

class Frame final : public TypedElement<internal::abstract::FrameElement> {
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

class Rect final : public TypedElement<internal::abstract::RectElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

class Line final : public TypedElement<internal::abstract::LineElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] GraphicStyle style() const;
};

class Circle final : public TypedElement<internal::abstract::CircleElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

class CustomShape final
    : public TypedElement<internal::abstract::CustomShapeElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] GraphicStyle style() const;
};

class Image final : public TypedElement<internal::abstract::ImageElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::optional<odr::File> file() const;
  [[nodiscard]] std::string href() const;
};

} // namespace odr

#endif // ODR_DOCUMENT_ELEMENT_H
