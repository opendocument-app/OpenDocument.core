#ifndef ODR_EXPERIMENTAL_DOCUMENT_ELEMENTS_H
#define ODR_EXPERIMENTAL_DOCUMENT_ELEMENTS_H

#include <any>
#include <memory>
#include <optional>
#include <string>

namespace odr::experimental {
enum class ElementProperty;
} // namespace odr::experimental

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::experimental {
class File;
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;
struct TableDimensions;
enum class ElementType;
class ElementPropertyValue;

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

class Element {
public:
  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] ElementPropertyValue
  property(experimental::ElementProperty property) const;

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

protected:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};

  Element();
  Element(std::shared_ptr<const internal::abstract::Document> impl,
          std::uint64_t id);

  friend Document;
  template <typename E> friend class ElementRangeTemplate;
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
  [[nodiscard]] SlideElement previous_sibling() const;
  [[nodiscard]] SlideElement next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::string notes() const;

private:
  SlideElement();
  SlideElement(std::shared_ptr<const internal::abstract::Document> impl,
               std::uint64_t id);

  friend Presentation;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class SheetElement final : public Element {
public:
  [[nodiscard]] SheetElement previous_sibling() const;
  [[nodiscard]] SheetElement next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] TableElement table() const;

private:
  SheetElement();
  SheetElement(std::shared_ptr<const internal::abstract::Document> impl,
               std::uint64_t id);

  friend Spreadsheet;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class PageElement final : public Element {
public:
  [[nodiscard]] PageElement previous_sibling() const;
  [[nodiscard]] PageElement next_sibling() const;

  [[nodiscard]] std::string name() const;

private:
  PageElement();
  PageElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Drawing;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class TextElement final : public Element {
public:
  [[nodiscard]] std::string string() const;

private:
  TextElement();
  TextElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class ParagraphElement final : public Element {
public:
private:
  ParagraphElement();
  ParagraphElement(std::shared_ptr<const internal::abstract::Document> impl,
                   std::uint64_t id);

  friend Element;
};

class SpanElement final : public Element {
public:
private:
  SpanElement();
  SpanElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class LinkElement final : public Element {
public:
  [[nodiscard]] std::string href() const;

private:
  LinkElement();
  LinkElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class BookmarkElement final : public Element {
public:
  [[nodiscard]] std::string name() const;

private:
  BookmarkElement();
  BookmarkElement(std::shared_ptr<const internal::abstract::Document> impl,
                  std::uint64_t id);

  friend Element;
};

class ListElement final : public Element {
public:
private:
  ListElement();
  ListElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class ListItemElement final : public Element {
public:
private:
  ListItemElement();
  ListItemElement(std::shared_ptr<const internal::abstract::Document> impl,
                  std::uint64_t id);

  friend Element;
};

class TableElement final : public Element {
public:
  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] TableColumnRange columns() const;
  [[nodiscard]] TableRowRange rows() const;

private:
  TableElement();
  TableElement(std::shared_ptr<const internal::abstract::Document> impl,
               std::uint64_t id);

  friend Element;
  friend SheetElement;
};

class TableColumnElement final : public Element {
public:
  [[nodiscard]] TableColumnElement previous_sibling() const;
  [[nodiscard]] TableColumnElement next_sibling() const;

private:
  TableColumnElement();
  TableColumnElement(std::shared_ptr<const internal::abstract::Document> impl,
                     std::uint64_t id);

  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class TableRowElement final : public Element {
public:
  [[nodiscard]] TableCellElement first_child() const;
  [[nodiscard]] TableRowElement previous_sibling() const;
  [[nodiscard]] TableRowElement next_sibling() const;

  [[nodiscard]] TableCellRange cells() const;

private:
  TableRowElement();
  TableRowElement(std::shared_ptr<const internal::abstract::Document> impl,
                  std::uint64_t id);

  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class TableCellElement final : public Element {
public:
  [[nodiscard]] TableCellElement previous_sibling() const;
  [[nodiscard]] TableCellElement next_sibling() const;

  [[nodiscard]] std::uint32_t row_span() const;
  [[nodiscard]] std::uint32_t column_span() const;

private:
  TableCellElement();
  TableCellElement(std::shared_ptr<const internal::abstract::Document> impl,
                   std::uint64_t id);

  friend Element;
  template <typename E> friend class ElementRangeTemplate;
  friend TableRowElement;
};

class FrameElement final : public Element {
public:
  [[nodiscard]] std::string anchor_type() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;
  [[nodiscard]] std::string z_index() const;

private:
  FrameElement();
  FrameElement(std::shared_ptr<const internal::abstract::Document> impl,
               std::uint64_t id);

  friend Element;
};

class ImageElement final : public Element {
public:
  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::string href() const;
  [[nodiscard]] File image_file() const;

private:
  ImageElement();
  ImageElement(std::shared_ptr<const internal::abstract::Document> impl,
               std::uint64_t id);

  friend Element;
};

class RectElement final : public Element {
public:
  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

private:
  RectElement();
  RectElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class LineElement final : public Element {
public:
  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

private:
  LineElement();
  LineElement(std::shared_ptr<const internal::abstract::Document> impl,
              std::uint64_t id);

  friend Element;
};

class CircleElement final : public Element {
public:
  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

private:
  CircleElement();
  CircleElement(std::shared_ptr<const internal::abstract::Document> impl,
                std::uint64_t id);

  friend Element;
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_DOCUMENT_ELEMENTS_H
