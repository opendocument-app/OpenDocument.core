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
class ElementSiblingRange;
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

class Element final {
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

  ElementSiblingRange children() const;

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

class ElementSiblingIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using pointer = Element *;
  using reference = Element &;

  explicit ElementSiblingIterator(Element element);

  ElementSiblingIterator &operator++();
  ElementSiblingIterator operator++(int) &;
  reference operator*();
  pointer operator->();
  bool operator==(const ElementSiblingIterator &rhs) const;
  bool operator!=(const ElementSiblingIterator &rhs) const;

private:
  Element m_element;
};

class ElementSiblingRange final {
public:
  ElementSiblingRange();
  explicit ElementSiblingRange(Element begin);
  ElementSiblingRange(Element begin, Element end);

  ElementSiblingIterator begin() const;
  ElementSiblingIterator end() const;

  Element front() const;

private:
  Element m_begin;
  Element m_end;
};

class TextElement final {
public:
  TextElement();
  explicit TextElement(std::shared_ptr<const common::TextElement> impl);

  explicit operator bool() const;

  std::string string() const;

private:
  std::shared_ptr<const common::TextElement> m_impl;
};

class ParagraphElement final {
public:
  ParagraphElement();
  explicit ParagraphElement(std::shared_ptr<const common::Paragraph> impl);

  explicit operator bool() const;

  ParagraphProperties paragraphProperties() const;
  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Paragraph> m_impl;
};

class SpanElement final {
public:
  SpanElement();
  explicit SpanElement(std::shared_ptr<const common::Span> impl);

  explicit operator bool() const;

  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Span> m_impl;
};

class LinkElement final {
public:
  LinkElement();
  explicit LinkElement(std::shared_ptr<const common::Link> impl);

  explicit operator bool() const;

  TextProperties textProperties() const;

  std::string href() const;

private:
  std::shared_ptr<const common::Link> m_impl;
};

class BookmarkElement final {
public:
  BookmarkElement();
  explicit BookmarkElement(std::shared_ptr<const common::Bookmark> impl);

  explicit operator bool() const;

  std::string name() const;

private:
  std::shared_ptr<const common::Bookmark> m_impl;
};

class ImageElement final {
public:
  ImageElement();
  explicit ImageElement(std::shared_ptr<const common::Image> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Image> m_impl;
};

class ListElement final {
public:
  ListElement();
  explicit ListElement(std::shared_ptr<const common::List> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::List> m_impl;
};

class ListItemElement final {
public:
  ListItemElement();
  explicit ListItemElement(std::shared_ptr<const common::ListItem> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::ListItem> m_impl;
};

class TableElement final {
public:
  TableElement();
  explicit TableElement(std::shared_ptr<const common::Table> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Table> m_impl;
};

class TableColumnElement final {
public:
  TableColumnElement();
  explicit TableColumnElement(std::shared_ptr<const common::TableColumn> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::TableColumn> m_impl;
};

class TableRowElement final {
public:
  TableRowElement();
  explicit TableRowElement(std::shared_ptr<const common::TableRow> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::TableRow> m_impl;
};

class TableCellElement final {
public:
  TableCellElement();
  explicit TableCellElement(std::shared_ptr<const common::TableCell> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::TableCell> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENTELEMENTS_H
