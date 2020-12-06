#ifndef ODR_DOCUMENTELEMENTS_H
#define ODR_DOCUMENTELEMENTS_H

#include <memory>
#include <optional>
#include <string>

namespace odr {
namespace common {
class Element;
class TextElement;
class Paragraph;
class Span;
class Link;
class Bookmark;
class Image;
class List;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
} // namespace common

class Element;
class TextElement;
class ParagraphElement;
class SpanElement;
class LinkElement;
class BookmarkElement;
class ImageElement;
class ListElement;
class ListItemElement;
class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;
template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
using TableColumnRange = ElementRangeTemplate<TableColumnElement>;
using TableRowRange = ElementRangeTemplate<TableRowElement>;
using TableCellRange = ElementRangeTemplate<TableCellElement>;

enum class ElementType {
  UNKNOWN,
  TEXT,
  LINE_BREAK,
  PAGE_BREAK,
  PARAGRAPH,
  SPAN,
  LINK,
  BOOKMARK,
  IMAGE,
  LIST,
  LIST_ITEM,
  TABLE,
  TABLE_COLUMN,
  TABLE_ROW,
  TABLE_CELL,
};

struct FontProperties {
  std::optional<std::string> font;
  std::optional<std::string> size;
  std::optional<std::string> weight;
  std::optional<std::string> style;
  std::optional<std::string> color;

  explicit operator bool() const;
};

struct RectangularProperties {
  std::optional<std::string> top;
  std::optional<std::string> bottom;
  std::optional<std::string> left;
  std::optional<std::string> right;

  explicit operator bool() const;
};

struct ContainerProperties {
  std::optional<std::string> width;
  std::optional<std::string> height;

  RectangularProperties margin;
  RectangularProperties padding;
  RectangularProperties border;

  std::optional<std::string> horizontalAlign;
  std::optional<std::string> verticalAlign;
};

struct ParagraphProperties {
  std::optional<std::string> textAlign;
  RectangularProperties margin;
};

struct TextProperties {
  FontProperties font;
  std::optional<std::string> backgroundColor;
};

struct TableProperties {
  std::optional<std::string> width;
};

struct TableColumnProperties {
  std::optional<std::string> width;
};

struct TableRowProperties {
};

struct TableCellProperties {
  std::optional<std::string> padding;
  RectangularProperties paddingRect;
  std::optional<std::string> border;
  RectangularProperties borderRect;
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
  TextElement text() const;
  Element lineBreak() const;
  Element pageBreak() const;
  ParagraphElement paragraph() const;
  SpanElement span() const;
  LinkElement link() const;
  BookmarkElement bookmark() const;
  ImageElement image() const;
  ListElement list() const;
  ListItemElement listItem() const;
  TableElement table() const;
  TableColumnElement tableColumn() const;
  TableRowElement tableRow() const;
  TableCellElement tableCell() const;

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

class ImageElement final : public Element {
public:
  ImageElement();
  explicit ImageElement(std::shared_ptr<const common::Image> impl);

private:
  std::shared_ptr<const common::Image> m_impl;
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

} // namespace odr

#endif // ODR_DOCUMENTELEMENTS_H
