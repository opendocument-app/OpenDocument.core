#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class Document;
class DocumentCursor;

class Element;
class TextElement;
class LinkElement;
class BookmarkElement;
class SlideElement;
class PageElement;
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

class Style;

class Property;
class DirectionalProperty;
} // namespace odr::internal::abstract

namespace odr {

enum class DocumentType;
class File;
class DocumentFile;
struct TableDimensions;

enum class ElementType {
  none,

  root,
  slide,
  sheet,
  page,

  master_page,

  text,
  line_break,
  // page_break, TODO
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

class DocumentCursor;
class Element;
struct PageLayout;
class Style;

class Document final {
public:
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] DocumentCursor root_element() const;

private:
  std::shared_ptr<internal::abstract::Document> m_document;

  explicit Document(std::shared_ptr<internal::abstract::Document> document);

  friend DocumentFile;
};

class DocumentCursor final {
public:
  bool operator==(const DocumentCursor &rhs) const;
  bool operator!=(const DocumentCursor &rhs) const;

  [[nodiscard]] std::string document_path() const;

  [[nodiscard]] ElementType element_type() const;

  bool move_to_parent();
  bool move_to_first_child();
  bool move_to_previous_sibling();
  bool move_to_next_sibling();

  [[nodiscard]] Element element() const;

  [[nodiscard]] bool move_to_master_page();

  [[nodiscard]] bool move_to_first_table_column();
  [[nodiscard]] bool move_to_first_table_row();

  using ChildVisitor =
      std::function<void(DocumentCursor &cursor, std::uint32_t i)>;
  using ConditionalChildVisitor =
      std::function<bool(DocumentCursor &cursor, std::uint32_t i)>;

  void for_each_child(const ChildVisitor &visitor);
  void for_each_column(const ConditionalChildVisitor &visitor);
  void for_each_row(const ConditionalChildVisitor &visitor);
  void for_each_cell(const ConditionalChildVisitor &visitor);

private:
  std::shared_ptr<internal::abstract::Document> m_document;
  std::shared_ptr<internal::abstract::DocumentCursor> m_cursor;

  DocumentCursor(std::shared_ptr<internal::abstract::Document> document,
                 std::shared_ptr<internal::abstract::DocumentCursor> cursor);

  void for_each_(const ChildVisitor &visitor);
  void for_each_(const ConditionalChildVisitor &visitor);

  friend Document;
};

class Element final {
public:
  Element(const internal::abstract::Document *document,
          internal::abstract::Element *element);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  template <typename T> class Extension {
  public:
    Extension(const internal::abstract::Document *document,
              internal::abstract::Element *element)
        : m_document{document}, m_element{dynamic_cast<T *>(element)} {}

    explicit operator bool() const { return m_element; }

  protected:
    const internal::abstract::Document *m_document;
    T *m_element;
  };

  class Slide final : public Extension<internal::abstract::SlideElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] PageLayout page_layout() const;
  };

  class Page final : public Extension<internal::abstract::PageElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] PageLayout page_layout() const;
  };

  class Text final : public Extension<internal::abstract::TextElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string value() const;
  };

  class Link final : public Extension<internal::abstract::LinkElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string href() const;
  };

  class Bookmark final : public Extension<internal::abstract::BookmarkElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string name() const;
  };

  class Table final : public Extension<internal::abstract::TableElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] TableDimensions dimensions() const;
  };

  class TableColumn final
      : public Extension<internal::abstract::TableColumnElement> {
  public:
    using Extension::Extension;
  };

  class TableRow final : public Extension<internal::abstract::TableRowElement> {
  public:
    using Extension::Extension;
  };

  class TableCell final
      : public Extension<internal::abstract::TableCellElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] TableDimensions span() const;
  };

  class Frame final : public Extension<internal::abstract::FrameElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<std::string> anchor_type() const;
    [[nodiscard]] std::optional<std::string> x() const;
    [[nodiscard]] std::optional<std::string> y() const;
    [[nodiscard]] std::optional<std::string> width() const;
    [[nodiscard]] std::optional<std::string> height() const;
    [[nodiscard]] std::optional<std::string> z_index() const;
  };

  class Rect final : public Extension<internal::abstract::RectElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string x() const;
    [[nodiscard]] std::string y() const;
    [[nodiscard]] std::string width() const;
    [[nodiscard]] std::string height() const;
  };

  class Line final : public Extension<internal::abstract::LineElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string x1() const;
    [[nodiscard]] std::string y1() const;
    [[nodiscard]] std::string x2() const;
    [[nodiscard]] std::string y2() const;
  };

  class Circle final : public Extension<internal::abstract::CircleElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string x() const;
    [[nodiscard]] std::string y() const;
    [[nodiscard]] std::string width() const;
    [[nodiscard]] std::string height() const;
  };

  class CustomShape final
      : public Extension<internal::abstract::CustomShapeElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<std::string> x() const;
    [[nodiscard]] std::optional<std::string> y() const;
    [[nodiscard]] std::string width() const;
    [[nodiscard]] std::string height() const;
  };

  class Image final : public Extension<internal::abstract::ImageElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] bool internal() const;
    [[nodiscard]] std::optional<odr::File> file() const;
    [[nodiscard]] std::string href() const;
  };

  [[nodiscard]] Text text() const;
  [[nodiscard]] Link link() const;
  [[nodiscard]] Bookmark bookmark() const;
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

private:
  const internal::abstract::Document *m_document;
  internal::abstract::Element *m_element;
};

template <typename T> struct DirectionalStyleProperty final {
  std::optional<T> right;
  std::optional<T> top;
  std::optional<T> left;
  std::optional<T> bottom;
};

struct TextStyle final {
  std::optional<std::string> font_name;
  std::optional<std::string> font_size;
  std::optional<std::string> font_weight;
  std::optional<std::string> font_style;
  std::optional<std::string> font_underline;
  std::optional<std::string> font_line_through;
  std::optional<std::string> font_shadow;
  std::optional<std::string> font_color;
  std::optional<std::string> background_color;
};

struct ParagraphStyle final {
  std::optional<std::string> text_align;
  DirectionalStyleProperty<std::string> margin;
};

struct TableStyle final {
  std::optional<std::string> width;
};

struct TableColumnStyle final {
  std::optional<std::string> width;
};

struct TableRowStyle final {
  std::optional<std::string> height;
};

struct TableCellStyle final {
  std::optional<std::string> vertical_align;
  std::optional<std::string> background_color;
  DirectionalStyleProperty<std::string> padding;
  DirectionalStyleProperty<std::string> border;
};

struct GraphicStyle final {
  std::optional<std::string> stroke_width;
  std::optional<std::string> stroke_color;
  std::optional<std::string> fill_color;
  std::optional<std::string> vertical_align;
};

struct PageLayout final {
  std::optional<std::string> width;
  std::optional<std::string> height;
  std::optional<std::string> print_orientation;
  DirectionalStyleProperty<std::string> margin;
};

class Style final {
public:
  Style(const internal::abstract::Document *document,
        const internal::abstract::Element *element,
        const internal::abstract::Style *style);

  explicit operator bool() const;

  [[nodiscard]] std::optional<std::string> name() const;

  [[nodiscard]] std::optional<TextStyle> text_style() const;
  [[nodiscard]] std::optional<ParagraphStyle> paragraph_style() const;
  [[nodiscard]] std::optional<TableStyle> table_style() const;
  [[nodiscard]] std::optional<TableColumnStyle> table_column_style() const;
  [[nodiscard]] std::optional<TableRowStyle> table_row_style() const;
  [[nodiscard]] std::optional<TableCellStyle> table_cell_style() const;
  [[nodiscard]] std::optional<GraphicStyle> graphic_style() const;

private:
  const internal::abstract::Document *m_document;
  const internal::abstract::Element *m_element;
  const internal::abstract::Style *m_style;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_DOCUMENT_H
