#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class Document;
class Table;
} // namespace odr::internal::abstract

namespace odr {
enum class DocumentType;

class File;
class DocumentFile;

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

  FRAME,
  IMAGE,
  RECT,
  LINE,
  CIRCLE,
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

  X,
  Y,
  WIDTH,
  HEIGHT,

  X1,
  Y1,
  X2,
  Y2,

  Z_INDEX,

  TABLE_CELL_ROW_SPAN,
  TABLE_CELL_COLUMN_SPAN,

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
  FONT_COLOR,

  BACKGROUND_COLOR,

  TEXT_ALIGN,

  STROKE_WIDTH,
  STROKE_COLOR,
  FILL_COLOR,
  VERTICAL_ALIGN,

  IMAGE_INTERNAL,
  IMAGE_FILE,
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

class PropertyValue {
public:
  virtual ~PropertyValue() = default;

  virtual explicit operator bool() const = 0;

  [[nodiscard]] virtual std::any get() const = 0;
  [[nodiscard]] std::string get_string() const;
  [[nodiscard]] std::uint32_t get_uint32() const;
  [[nodiscard]] bool get_bool() const;
  [[nodiscard]] const char *get_optional_string() const;

  virtual void set(const std::any &value) const = 0;
  void set_string(const std::string &value) const;

  virtual void remove() const = 0;
};

class ElementPropertyValue final : public PropertyValue {
public:
  bool operator==(const ElementPropertyValue &rhs) const;
  bool operator!=(const ElementPropertyValue &rhs) const;
  explicit operator bool() const final;

  [[nodiscard]] std::any get() const final;

  void set(const std::any &value) const final;

  void remove() const final;

private:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};
  ElementProperty m_property{ElementProperty::NONE};

  ElementPropertyValue();
  ElementPropertyValue(std::shared_ptr<const internal::abstract::Document> impl,
                       std::uint64_t id, ElementProperty property);

  friend Element;
  friend PageStyle;
};

class TableColumnPropertyValue final : public PropertyValue {
public:
  bool operator==(const TableColumnPropertyValue &rhs) const;
  bool operator!=(const TableColumnPropertyValue &rhs) const;
  explicit operator bool() const final;

  [[nodiscard]] std::any get() const final;

  void set(const std::any &value) const final;

  void remove() const final;

private:
  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_column{0};
  ElementProperty m_property{ElementProperty::NONE};

  TableColumnPropertyValue();
  TableColumnPropertyValue(
      std::shared_ptr<const internal::abstract::Table> impl,
      std::uint32_t column, ElementProperty property);

  friend TableColumnElement;
};

class TableRowPropertyValue final : public PropertyValue {
public:
  bool operator==(const TableRowPropertyValue &rhs) const;
  bool operator!=(const TableRowPropertyValue &rhs) const;
  explicit operator bool() const final;

  [[nodiscard]] std::any get() const final;

  void set(const std::any &value) const final;

  void remove() const final;

private:
  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};
  ElementProperty m_property{ElementProperty::NONE};

  TableRowPropertyValue();
  TableRowPropertyValue(std::shared_ptr<const internal::abstract::Table> impl,
                        std::uint32_t row, ElementProperty property);

  friend TableRowElement;
};

class TableCellPropertyValue final : public PropertyValue {
public:
  bool operator==(const TableCellPropertyValue &rhs) const;
  bool operator!=(const TableCellPropertyValue &rhs) const;
  explicit operator bool() const final;

  [[nodiscard]] std::any get() const final;

  void set(const std::any &value) const final;

  void remove() const final;

private:
  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};
  std::uint32_t m_column{0};
  ElementProperty m_property{ElementProperty::NONE};

  TableCellPropertyValue();
  TableCellPropertyValue(std::shared_ptr<const internal::abstract::Table> impl,
                         std::uint32_t row, std::uint32_t column,
                         ElementProperty property);

  friend TableCellElement;
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
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};

  PageStyle();
  PageStyle(std::shared_ptr<const internal::abstract::Document> impl,
            std::uint64_t id);

  friend TextDocument;
  friend SlideElement;
  friend PageElement;
};

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

  [[nodiscard]] ElementPropertyValue property(ElementProperty property) const;

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
  friend class TableCellElement;
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

  [[nodiscard]] PageStyle page_style() const;

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

  [[nodiscard]] PageStyle page_style() const;

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

  std::shared_ptr<const internal::abstract::Table> m_table;

  friend Element;
  friend SheetElement;
};

class TableColumnElement final {
public:
  bool operator==(const TableColumnElement &rhs) const;
  bool operator!=(const TableColumnElement &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableColumnElement previous_sibling() const;
  [[nodiscard]] TableColumnElement next_sibling() const;

  [[nodiscard]] TableColumnPropertyValue
  property(ElementProperty property) const;

private:
  TableColumnElement();
  TableColumnElement(std::shared_ptr<const internal::abstract::Table> impl,
                     std::uint32_t column);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_column{0};

  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class TableRowElement final {
public:
  bool operator==(const TableRowElement &rhs) const;
  bool operator!=(const TableRowElement &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableCellElement first_child() const;
  [[nodiscard]] TableRowElement previous_sibling() const;
  [[nodiscard]] TableRowElement next_sibling() const;

  [[nodiscard]] TableRowPropertyValue property(ElementProperty property) const;

  [[nodiscard]] TableCellRange cells() const;

private:
  TableRowElement();
  TableRowElement(std::shared_ptr<const internal::abstract::Table> impl,
                  std::uint32_t row);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};

  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class TableCellElement final {
public:
  bool operator==(const TableCellElement &rhs) const;
  bool operator!=(const TableCellElement &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableCellElement previous_sibling() const;
  [[nodiscard]] TableCellElement next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] TableCellPropertyValue property(ElementProperty property) const;

  [[nodiscard]] std::uint32_t row_span() const;
  [[nodiscard]] std::uint32_t column_span() const;

private:
  TableCellElement();
  TableCellElement(std::shared_ptr<const internal::abstract::Table> impl,
                   std::uint32_t row, std::uint32_t column);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};
  std::uint32_t m_column{0};

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

class Document {
public:
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] Element root() const;

  [[nodiscard]] TextDocument text_document() const;
  [[nodiscard]] Presentation presentation() const;
  [[nodiscard]] Spreadsheet spreadsheet() const;
  [[nodiscard]] Drawing drawing() const;

protected:
  std::shared_ptr<internal::abstract::Document> m_impl;

  explicit Document(std::shared_ptr<internal::abstract::Document>);

private:
  friend DocumentFile;
};

class TextDocument final : public Document {
public:
  [[nodiscard]] ElementRange content() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  explicit TextDocument(std::shared_ptr<internal::abstract::Document>);

  friend Document;
};

class Presentation final : public Document {
public:
  [[nodiscard]] std::uint32_t slide_count() const;

  [[nodiscard]] SlideRange slides() const;

private:
  explicit Presentation(std::shared_ptr<internal::abstract::Document>);

  friend Document;
};

class Spreadsheet final : public Document {
public:
  [[nodiscard]] std::uint32_t sheet_count() const;

  [[nodiscard]] SheetRange sheets() const;

private:
  explicit Spreadsheet(std::shared_ptr<internal::abstract::Document>);

  friend Document;
};

class Drawing final : public Document {
public:
  [[nodiscard]] std::uint32_t page_count() const;

  [[nodiscard]] PageRange pages() const;

private:
  explicit Drawing(std::shared_ptr<internal::abstract::Document>);

  friend Document;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
