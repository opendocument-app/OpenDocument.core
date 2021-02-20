#ifndef ODR_DOCUMENT_ELEMENTS_H
#define ODR_DOCUMENT_ELEMENTS_H

#include <memory>
#include <odr/property.h>
#include <optional>
#include <string>

namespace odr::abstract {
class Element;
class Slide;
class Sheet;
class Page;
class TextElement;
class Paragraph;
class Span;
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
class DrawingElement;
class Rect;
class Line;
class Circle;
} // namespace odr::abstract

namespace odr {
class File;

class PageStyle;
class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;

class Element;
class SlideElement;
class SheetElement;
class PageElement;
class TextElement;
class ParagraphElement;
class SpanElement;
class LinkElement;
class BookmarkElement;
class ListElement;
class ListItemElement;
class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;
class FrameElement;
class ImageElement;
class RectElement;
class LineElement;
class CircleElement;
template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
using SlideRange = ElementRangeTemplate<SlideElement>;
using SheetRange = ElementRangeTemplate<SheetElement>;
using PageRange = ElementRangeTemplate<PageElement>;
using TableColumnRange = ElementRangeTemplate<TableColumnElement>;
using TableRowRange = ElementRangeTemplate<TableRowElement>;
using TableCellRange = ElementRangeTemplate<TableCellElement>;

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
};

class Element {
public:
  Element();
  explicit Element(std::shared_ptr<const abstract::Element> impl);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] SlideElement slide() const;
  [[nodiscard]] SheetElement sheet() const;
  [[nodiscard]] PageElement page() const;
  [[nodiscard]] TextElement text() const;
  [[nodiscard]] Element line_break() const;
  [[nodiscard]] Element page_break() const;
  [[nodiscard]] ParagraphElement paragraph() const;
  [[nodiscard]] SpanElement span() const;
  [[nodiscard]] LinkElement link() const;
  [[nodiscard]] BookmarkElement bookmark() const;
  [[nodiscard]] ListElement list() const;
  [[nodiscard]] ListItemElement list_item() const;
  [[nodiscard]] TableElement table() const;
  [[nodiscard]] TableColumnElement table_column() const;
  [[nodiscard]] TableRowElement table_row() const;
  [[nodiscard]] TableCellElement table_cell() const;
  [[nodiscard]] FrameElement frame() const;
  [[nodiscard]] ImageElement image() const;
  [[nodiscard]] RectElement rect() const;
  [[nodiscard]] LineElement line() const;
  [[nodiscard]] CircleElement circle() const;

private:
  std::shared_ptr<const abstract::Element> m_impl;
};

template <typename E> class ElementIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = E;
  using difference_type = std::ptrdiff_t;
  using pointer = E *;
  using reference = E &;

  explicit ElementIterator(E element);

  ElementIterator<E> &operator++();
  ElementIterator<E> operator++(int) &;
  reference operator*();
  pointer operator->();
  bool operator==(const ElementIterator<E> &rhs) const;
  bool operator!=(const ElementIterator<E> &rhs) const;

private:
  E m_element;
};

template <typename E> class ElementRangeTemplate final {
public:
  ElementRangeTemplate();
  explicit ElementRangeTemplate(E begin);
  ElementRangeTemplate(E begin, E end);

  [[nodiscard]] ElementIterator<E> begin() const;
  [[nodiscard]] ElementIterator<E> end() const;

  [[nodiscard]] E front() const;

private:
  E m_begin;
  E m_end;
};

class SlideElement final : public Element {
public:
  SlideElement();
  explicit SlideElement(std::shared_ptr<const abstract::Slide> impl);

  [[nodiscard]] SlideElement previous_sibling() const;
  [[nodiscard]] SlideElement next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::string notes() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  std::shared_ptr<const abstract::Slide> m_impl;
};

class SheetElement final : public Element {
public:
  SheetElement();
  explicit SheetElement(std::shared_ptr<const abstract::Sheet> impl);

  [[nodiscard]] SheetElement previous_sibling() const;
  [[nodiscard]] SheetElement next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::uint32_t row_count() const;
  [[nodiscard]] std::uint32_t column_count() const;
  [[nodiscard]] TableElement table() const;

private:
  std::shared_ptr<const abstract::Sheet> m_impl;
};

class PageElement final : public Element {
public:
  PageElement();
  explicit PageElement(std::shared_ptr<const abstract::Page> impl);

  [[nodiscard]] PageElement previous_sibling() const;
  [[nodiscard]] PageElement next_sibling() const;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  std::shared_ptr<const abstract::Page> m_impl;
};

class TextElement final : public Element {
public:
  TextElement();
  explicit TextElement(std::shared_ptr<const abstract::TextElement> impl);

  [[nodiscard]] std::string string() const;

private:
  std::shared_ptr<const abstract::TextElement> m_impl;
};

class ParagraphElement final : public Element {
public:
  ParagraphElement();
  explicit ParagraphElement(std::shared_ptr<const abstract::Paragraph> impl);

  [[nodiscard]] ParagraphStyle paragraph_style() const;
  [[nodiscard]] TextStyle text_style() const;

private:
  std::shared_ptr<const abstract::Paragraph> m_impl;
};

class SpanElement final : public Element {
public:
  SpanElement();
  explicit SpanElement(std::shared_ptr<const abstract::Span> impl);

  [[nodiscard]] TextStyle text_style() const;

private:
  std::shared_ptr<const abstract::Span> m_impl;
};

class LinkElement final : public Element {
public:
  LinkElement();
  explicit LinkElement(std::shared_ptr<const abstract::Link> impl);

  [[nodiscard]] TextStyle text_style() const;

  [[nodiscard]] std::string href() const;

private:
  std::shared_ptr<const abstract::Link> m_impl;
};

class BookmarkElement final : public Element {
public:
  BookmarkElement();
  explicit BookmarkElement(std::shared_ptr<const abstract::Bookmark> impl);

  [[nodiscard]] std::string name() const;

private:
  std::shared_ptr<const abstract::Bookmark> m_impl;
};

class ListElement final : public Element {
public:
  ListElement();
  explicit ListElement(std::shared_ptr<const abstract::List> impl);

private:
  std::shared_ptr<const abstract::List> m_impl;
};

class ListItemElement final : public Element {
public:
  ListItemElement();
  explicit ListItemElement(std::shared_ptr<const abstract::ListItem> impl);

private:
  std::shared_ptr<const abstract::ListItem> m_impl;
};

class TableElement final : public Element {
public:
  TableElement();
  explicit TableElement(std::shared_ptr<const abstract::Table> impl);

  [[nodiscard]] TableColumnRange columns() const;
  [[nodiscard]] TableRowRange rows() const;

  [[nodiscard]] TableStyle table_style() const;

private:
  std::shared_ptr<const abstract::Table> m_impl;
};

class TableColumnElement final : public Element {
public:
  TableColumnElement();
  explicit TableColumnElement(
      std::shared_ptr<const abstract::TableColumn> impl);

  [[nodiscard]] TableColumnElement previous_sibling() const;
  [[nodiscard]] TableColumnElement next_sibling() const;

  [[nodiscard]] TableColumnStyle table_column_style() const;

private:
  std::shared_ptr<const abstract::TableColumn> m_impl;
};

class TableRowElement final : public Element {
public:
  TableRowElement();
  explicit TableRowElement(std::shared_ptr<const abstract::TableRow> impl);

  [[nodiscard]] TableCellElement first_child() const;
  [[nodiscard]] TableRowElement previous_sibling() const;
  [[nodiscard]] TableRowElement next_sibling() const;

  [[nodiscard]] TableCellRange cells() const;

private:
  std::shared_ptr<const abstract::TableRow> m_impl;
};

class TableCellElement final : public Element {
public:
  TableCellElement();
  explicit TableCellElement(std::shared_ptr<const abstract::TableCell> impl);

  [[nodiscard]] TableCellElement previous_sibling() const;
  [[nodiscard]] TableCellElement next_sibling() const;

  [[nodiscard]] std::uint32_t row_span() const;
  [[nodiscard]] std::uint32_t column_span() const;

  [[nodiscard]] TableCellStyle table_cell_style() const;

private:
  std::shared_ptr<const abstract::TableCell> m_impl;
};

class FrameElement final : public Element {
public:
  FrameElement();
  explicit FrameElement(std::shared_ptr<const abstract::Frame> impl);

  [[nodiscard]] Property anchor_type() const;
  [[nodiscard]] Property width() const;
  [[nodiscard]] Property height() const;
  [[nodiscard]] Property z_index() const;

private:
  std::shared_ptr<const abstract::Frame> m_impl;
};

class ImageElement final : public Element {
public:
  ImageElement();
  explicit ImageElement(std::shared_ptr<const abstract::Image> impl);

  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::string href() const;
  [[nodiscard]] File image_file() const;

private:
  std::shared_ptr<const abstract::Image> m_impl;
};

class RectElement final : public Element {
public:
  RectElement();
  explicit RectElement(std::shared_ptr<const abstract::Rect> impl);

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] DrawingStyle drawing_style() const;

private:
  std::shared_ptr<const abstract::Rect> m_impl;
};

class LineElement final : public Element {
public:
  LineElement();
  explicit LineElement(std::shared_ptr<const abstract::Line> impl);

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] DrawingStyle drawing_style() const;

private:
  std::shared_ptr<const abstract::Line> m_impl;
};

class CircleElement final : public Element {
public:
  CircleElement();
  explicit CircleElement(std::shared_ptr<const abstract::Circle> impl);

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] DrawingStyle drawing_style() const;

private:
  std::shared_ptr<const abstract::Circle> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENT_ELEMENTS_H
