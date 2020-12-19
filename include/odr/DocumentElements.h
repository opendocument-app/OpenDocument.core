#ifndef ODR_DOCUMENTELEMENTS_H
#define ODR_DOCUMENTELEMENTS_H

#include <memory>
#include <odr/Property.h>
#include <optional>
#include <string>

namespace odr {
class ImageFile;

namespace common {
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
class Rect;
class Line;
class Circle;
} // namespace common

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
  UNKNOWN,

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

struct PageProperties {
  Property width;
  Property height;
  Property marginTop;
  Property marginBottom;
  Property marginLeft;
  Property marginRight;
  Property printOrientation;
};

struct FontProperties {
  Property font;
  Property size;
  Property weight;
  Property style;
  Property color;

  explicit operator bool() const;
};

struct RectangularProperties {
  Property top;
  Property bottom;
  Property left;
  Property right;

  explicit operator bool() const;
};

struct ContainerProperties {
  Property width;
  Property height;

  RectangularProperties margin;
  RectangularProperties padding;
  RectangularProperties border;

  Property horizontalAlign;
  Property verticalAlign;
};

struct ParagraphProperties {
  Property textAlign;
  RectangularProperties margin;
};

struct TextProperties {
  FontProperties font;
  Property backgroundColor;
};

struct TableProperties {
  Property width;
};

struct TableColumnProperties {
  Property width;
};

struct TableRowProperties {};

struct TableCellProperties {
  Property padding;
  RectangularProperties paddingRect;
  Property border;
  RectangularProperties borderRect;
};

struct FrameProperties {
  Property anchorType;
  Property width;
  Property height;
  Property zIndex;
};

struct DrawingProperties {
  Property strokeWidth;
  Property strokeColor;
  Property fillColor;
  Property verticalAlign;
};

class Element {
public:
  Element();
  explicit Element(std::shared_ptr<const common::Element> impl);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  Element parent() const;
  Element firstChild() const;
  Element previousSibling() const;
  Element nextSibling() const;

  ElementRange children() const;

  ElementType type() const;

  Element unknown() const;
  SlideElement slide() const;
  SheetElement sheet() const;
  PageElement page() const;
  TextElement text() const;
  Element lineBreak() const;
  Element pageBreak() const;
  ParagraphElement paragraph() const;
  SpanElement span() const;
  LinkElement link() const;
  BookmarkElement bookmark() const;
  ListElement list() const;
  ListItemElement listItem() const;
  TableElement table() const;
  TableColumnElement tableColumn() const;
  TableRowElement tableRow() const;
  TableCellElement tableCell() const;
  FrameElement frame() const;
  ImageElement image() const;
  RectElement rect() const;
  LineElement line() const;
  CircleElement circle() const;

private:
  std::shared_ptr<const common::Element> m_impl;
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

  ElementIterator<E> begin() const;
  ElementIterator<E> end() const;

  E front() const;

private:
  E m_begin;
  E m_end;
};

class SlideElement final : public Element {
public:
  SlideElement();
  explicit SlideElement(std::shared_ptr<const common::Slide> impl);

  std::string name() const;
  std::string notes() const;
  PageProperties pageProperties() const;

private:
  std::shared_ptr<const common::Slide> m_impl;
};

class SheetElement final : public Element {
public:
  SheetElement();
  explicit SheetElement(std::shared_ptr<const common::Sheet> impl);

  std::string name() const;
  std::uint32_t rowCount() const;
  std::uint32_t columnCount() const;
  TableElement table() const;

private:
  std::shared_ptr<const common::Sheet> m_impl;
};

class PageElement final : public Element {
public:
  PageElement();
  explicit PageElement(std::shared_ptr<const common::Page> impl);

  std::string name() const;
  PageProperties pageProperties() const;

private:
  std::shared_ptr<const common::Page> m_impl;
};

class TextElement final : public Element {
public:
  TextElement();
  explicit TextElement(std::shared_ptr<const common::TextElement> impl);

  std::string string() const;

private:
  std::shared_ptr<const common::TextElement> m_impl;
};

class ParagraphElement final : public Element {
public:
  ParagraphElement();
  explicit ParagraphElement(std::shared_ptr<const common::Paragraph> impl);

  ParagraphProperties paragraphProperties() const;
  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Paragraph> m_impl;
};

class SpanElement final : public Element {
public:
  SpanElement();
  explicit SpanElement(std::shared_ptr<const common::Span> impl);

  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Span> m_impl;
};

class LinkElement final : public Element {
public:
  LinkElement();
  explicit LinkElement(std::shared_ptr<const common::Link> impl);

  TextProperties textProperties() const;

  std::string href() const;

private:
  std::shared_ptr<const common::Link> m_impl;
};

class BookmarkElement final : public Element {
public:
  BookmarkElement();
  explicit BookmarkElement(std::shared_ptr<const common::Bookmark> impl);

  std::string name() const;

private:
  std::shared_ptr<const common::Bookmark> m_impl;
};

class ListElement final : public Element {
public:
  ListElement();
  explicit ListElement(std::shared_ptr<const common::List> impl);

private:
  std::shared_ptr<const common::List> m_impl;
};

class ListItemElement final : public Element {
public:
  ListItemElement();
  explicit ListItemElement(std::shared_ptr<const common::ListItem> impl);

private:
  std::shared_ptr<const common::ListItem> m_impl;
};

class TableElement final : public Element {
public:
  TableElement();
  explicit TableElement(std::shared_ptr<const common::Table> impl);

  TableColumnRange columns() const;
  TableRowRange rows() const;

  TableProperties tableProperties() const;

private:
  std::shared_ptr<const common::Table> m_impl;
};

class TableColumnElement final : public Element {
public:
  TableColumnElement();
  explicit TableColumnElement(std::shared_ptr<const common::TableColumn> impl);

  TableColumnElement previousSibling() const;
  TableColumnElement nextSibling() const;

  TableColumnProperties tableColumnProperties() const;

private:
  std::shared_ptr<const common::TableColumn> m_impl;
};

class TableRowElement final : public Element {
public:
  TableRowElement();
  explicit TableRowElement(std::shared_ptr<const common::TableRow> impl);

  TableCellElement firstChild() const;
  TableRowElement previousSibling() const;
  TableRowElement nextSibling() const;

  TableCellRange cells() const;

  TableRowProperties tableRowProperties() const;

private:
  std::shared_ptr<const common::TableRow> m_impl;
};

class TableCellElement final : public Element {
public:
  TableCellElement();
  explicit TableCellElement(std::shared_ptr<const common::TableCell> impl);

  TableCellElement previousSibling() const;
  TableCellElement nextSibling() const;

  std::uint32_t rowSpan() const;
  std::uint32_t columnSpan() const;

  TableCellProperties tableCellProperties() const;

private:
  std::shared_ptr<const common::TableCell> m_impl;
};

class FrameElement final : public Element {
public:
  FrameElement();
  explicit FrameElement(std::shared_ptr<const common::Frame> impl);

  FrameProperties frameProperties() const;

private:
  std::shared_ptr<const common::Frame> m_impl;
};

class ImageElement final : public Element {
public:
  ImageElement();
  explicit ImageElement(std::shared_ptr<const common::Image> impl);

  bool internal() const;
  std::string href() const;
  ImageFile imageFile() const;

private:
  std::shared_ptr<const common::Image> m_impl;
};

class RectElement final : public Element {
public:
  RectElement();
  explicit RectElement(std::shared_ptr<const common::Rect> impl);

  std::string x() const;
  std::string y() const;
  std::string width() const;
  std::string height() const;

  DrawingProperties drawingProperties() const;

private:
  std::shared_ptr<const common::Rect> m_impl;
};

class LineElement final : public Element {
public:
  LineElement();
  explicit LineElement(std::shared_ptr<const common::Line> impl);

  std::string x1() const;
  std::string y1() const;
  std::string x2() const;
  std::string y2() const;

  DrawingProperties drawingProperties() const;

private:
  std::shared_ptr<const common::Line> m_impl;
};

class CircleElement final : public Element {
public:
  CircleElement();
  explicit CircleElement(std::shared_ptr<const common::Circle> impl);

  std::string x() const;
  std::string y() const;
  std::string width() const;
  std::string height() const;

  DrawingProperties drawingProperties() const;

private:
  std::shared_ptr<const common::Circle> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENTELEMENTS_H
