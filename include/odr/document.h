#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
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
class Slide;
class Sheet;
class Page;
class Text;
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
class CustomShape;

template <typename E> class ElementRangeTemplate;
using ElementRange = ElementRangeTemplate<Element>;
using SlideRange = ElementRangeTemplate<Slide>;
using SheetRange = ElementRangeTemplate<Sheet>;
using PageRange = ElementRangeTemplate<Page>;
using TableColumnRange = ElementRangeTemplate<TableColumn>;
using TableRowRange = ElementRangeTemplate<TableRow>;
using TableCellRange = ElementRangeTemplate<TableCell>;

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
  TABLE_COLUMN,
  TABLE_ROW,
  TABLE_CELL,

  FRAME,
  IMAGE,
  RECT,
  LINE,
  CIRCLE,
  CUSTOM_SHAPE,
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

  STYLE_NAME,
  MASTER_PAGE_NAME,

  VALUE_TYPE,

  X,
  Y,
  WIDTH,
  HEIGHT,

  X1,
  Y1,
  X2,
  Y2,

  Z_INDEX,

  TABLE_COLUMN_DEFAULT_CELL_STYLE_NAME,
  TABLE_CELL_BACKGROUND_COLOR,

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
  FONT_UNDERLINE,
  FONT_STRIKETHROUGH,
  FONT_SHADOW,
  FONT_COLOR,

  BACKGROUND_COLOR,

  TEXT_ALIGN,

  STROKE_WIDTH,
  STROKE_COLOR,
  FILL_COLOR,
  VERTICAL_ALIGN,
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
  friend Frame;
  friend Image;
  friend Rect;
  friend Line;
  friend Circle;
  friend CustomShape;
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

  friend TableColumn;
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

  friend TableRow;
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

  friend TableCell;
};

class PropertySet {
public:
  [[nodiscard]] std::any get(ElementProperty property) const;
  [[nodiscard]] std::optional<std::string>
  get_string(ElementProperty property) const;
  [[nodiscard]] std::optional<std::uint32_t>
  get_uint32(ElementProperty property) const;
  [[nodiscard]] std::optional<bool> get_bool(ElementProperty property) const;

private:
  explicit PropertySet(
      std::unordered_map<ElementProperty, std::any> properties);

  std::unordered_map<ElementProperty, std::any> m_properties;

  friend Element;
  friend Frame;
  friend Image;
  friend Rect;
  friend Line;
  friend Circle;
  friend CustomShape;
  friend PageStyle;
  friend TableColumn;
  friend TableRow;
  friend TableCell;
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
  friend Slide;
  friend Page;
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

  [[nodiscard]] PropertySet properties() const;
  [[nodiscard]] ElementPropertyValue property(ElementProperty property) const;

  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
  [[nodiscard]] Text text() const;
  [[nodiscard]] Element line_break() const;
  [[nodiscard]] Element page_break() const;
  [[nodiscard]] Paragraph paragraph() const;
  [[nodiscard]] Span span() const;
  [[nodiscard]] Link link() const;
  [[nodiscard]] Bookmark bookmark() const;
  [[nodiscard]] List list() const;
  [[nodiscard]] ListItem list_item() const;
  [[nodiscard]] Table table() const;
  [[nodiscard]] Frame frame() const;
  [[nodiscard]] Image image() const;
  [[nodiscard]] Rect rect() const;
  [[nodiscard]] Line line() const;
  [[nodiscard]] Circle circle() const;
  [[nodiscard]] CustomShape custom_shape() const;

protected:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};

  Element();
  Element(std::shared_ptr<const internal::abstract::Document> impl,
          std::uint64_t id);

  friend Document;
  template <typename E> friend class ElementRangeTemplate;
  friend class TableCell;
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

class Slide final : public Element {
public:
  [[nodiscard]] Slide previous_sibling() const;
  [[nodiscard]] Slide next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::string notes() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  Slide();
  Slide(std::shared_ptr<const internal::abstract::Document> impl,
        std::uint64_t id);

  friend Presentation;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Sheet final : public Element {
public:
  [[nodiscard]] Sheet previous_sibling() const;
  [[nodiscard]] Sheet next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] Table table() const;

private:
  Sheet();
  Sheet(std::shared_ptr<const internal::abstract::Document> impl,
        std::uint64_t id);

  friend Spreadsheet;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Page final : public Element {
public:
  [[nodiscard]] Page previous_sibling() const;
  [[nodiscard]] Page next_sibling() const;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageStyle page_style() const;

private:
  Page();
  Page(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Drawing;
  friend Element;
  template <typename E> friend class ElementRangeTemplate;
};

class Text final : public Element {
public:
  [[nodiscard]] std::string string() const;

private:
  Text();
  Text(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class Paragraph final : public Element {
public:
private:
  Paragraph();
  Paragraph(std::shared_ptr<const internal::abstract::Document> impl,
            std::uint64_t id);

  friend Element;
};

class Span final : public Element {
public:
private:
  Span();
  Span(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class Link final : public Element {
public:
  [[nodiscard]] std::string href() const;

private:
  Link();
  Link(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class Bookmark final : public Element {
public:
  [[nodiscard]] std::string name() const;

private:
  Bookmark();
  Bookmark(std::shared_ptr<const internal::abstract::Document> impl,
           std::uint64_t id);

  friend Element;
};

class List final : public Element {
public:
private:
  List();
  List(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class ListItem final : public Element {
public:
private:
  ListItem();
  ListItem(std::shared_ptr<const internal::abstract::Document> impl,
           std::uint64_t id);

  friend Element;
};

class Table final : public Element {
public:
  [[nodiscard]] TableDimensions dimensions() const;
  [[nodiscard]] TableDimensions content_bounds() const;

  [[nodiscard]] TableColumnRange columns() const;
  [[nodiscard]] TableRowRange rows() const;

private:
  Table();
  Table(std::shared_ptr<const internal::abstract::Document> impl,
        std::uint64_t id);

  std::shared_ptr<const internal::abstract::Table> m_table;

  friend Element;
  friend Sheet;
};

class TableColumn final {
public:
  bool operator==(const TableColumn &rhs) const;
  bool operator!=(const TableColumn &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableColumn previous_sibling() const;
  [[nodiscard]] TableColumn next_sibling() const;

  [[nodiscard]] PropertySet properties() const;
  [[nodiscard]] TableColumnPropertyValue
  property(ElementProperty property) const;

private:
  TableColumn();
  TableColumn(std::shared_ptr<const internal::abstract::Table> impl,
              std::uint32_t column);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_column{0};

  friend Table;
  template <typename E> friend class ElementRangeTemplate;
};

class TableRow final {
public:
  bool operator==(const TableRow &rhs) const;
  bool operator!=(const TableRow &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableCell first_child() const;
  [[nodiscard]] TableRow previous_sibling() const;
  [[nodiscard]] TableRow next_sibling() const;

  [[nodiscard]] PropertySet properties() const;
  [[nodiscard]] TableRowPropertyValue property(ElementProperty property) const;

  [[nodiscard]] TableCellRange cells() const;

private:
  TableRow();
  TableRow(std::shared_ptr<const internal::abstract::Table> impl,
           std::uint32_t row);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};

  friend Table;
  template <typename E> friend class ElementRangeTemplate;
};

class TableCell final {
public:
  bool operator==(const TableCell &rhs) const;
  bool operator!=(const TableCell &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] TableCell previous_sibling() const;
  [[nodiscard]] TableCell next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] PropertySet properties() const;
  [[nodiscard]] TableCellPropertyValue property(ElementProperty property) const;

  [[nodiscard]] std::uint32_t row_span() const;
  [[nodiscard]] std::uint32_t column_span() const;

private:
  TableCell();
  TableCell(std::shared_ptr<const internal::abstract::Table> impl,
            std::uint32_t row, std::uint32_t column);

  std::shared_ptr<const internal::abstract::Table> m_impl;
  std::uint32_t m_row{0};
  std::uint32_t m_column{0};

  friend TableRow;
  template <typename E> friend class ElementRangeTemplate;
};

class Frame final : public Element {
public:
  [[nodiscard]] ElementPropertyValue anchor_type() const;
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;
  [[nodiscard]] ElementPropertyValue z_index() const;

private:
  Frame();
  Frame(std::shared_ptr<const internal::abstract::Document> impl,
        std::uint64_t id);

  friend Element;
};

class Image final : public Element {
public:
  [[nodiscard]] bool internal() const;
  [[nodiscard]] ElementPropertyValue href() const;
  [[nodiscard]] File image_file() const;

private:
  Image();
  Image(std::shared_ptr<const internal::abstract::Document> impl,
        std::uint64_t id);

  friend Element;
};

class Rect final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  Rect();
  Rect(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class Line final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x1() const;
  [[nodiscard]] ElementPropertyValue y1() const;
  [[nodiscard]] ElementPropertyValue x2() const;
  [[nodiscard]] ElementPropertyValue y2() const;

private:
  Line();
  Line(std::shared_ptr<const internal::abstract::Document> impl,
       std::uint64_t id);

  friend Element;
};

class Circle final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  Circle();
  Circle(std::shared_ptr<const internal::abstract::Document> impl,
         std::uint64_t id);

  friend Element;
};

class CustomShape final : public Element {
public:
  [[nodiscard]] ElementPropertyValue x() const;
  [[nodiscard]] ElementPropertyValue y() const;
  [[nodiscard]] ElementPropertyValue width() const;
  [[nodiscard]] ElementPropertyValue height() const;

private:
  CustomShape();
  CustomShape(std::shared_ptr<const internal::abstract::Document> impl,
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
