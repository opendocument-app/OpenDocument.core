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
class TextRootElement;
class SlideElement;
class SheetElement;
class PageElement;
class ParagraphElement;
class TextElement;
class LinkElement;
class BookmarkElement;
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

struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;

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
          const internal::abstract::DocumentCursor *cursor,
          internal::abstract::Element *element);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  template <typename T> class Extension {
  public:
    Extension(const internal::abstract::Document *document,
              const internal::abstract::DocumentCursor *cursor,
              internal::abstract::Element *element)
        : m_document{document}, m_cursor{cursor}, m_element{dynamic_cast<T *>(
                                                      element)} {}

    explicit operator bool() const { return m_element; }

  protected:
    const internal::abstract::Document *m_document;
    const internal::abstract::DocumentCursor *m_cursor;
    T *m_element;
  };

  class TextRoot final : public Extension<internal::abstract::TextRootElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] PageLayout page_layout() const;
  };

  class Slide final : public Extension<internal::abstract::SlideElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string name() const;

    [[nodiscard]] PageLayout page_layout() const;
  };

  class Sheet final : public Extension<internal::abstract::SheetElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string name() const;
  };

  class Page final : public Extension<internal::abstract::PageElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] PageLayout page_layout() const;
  };

  class Paragraph final
      : public Extension<internal::abstract::ParagraphElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<ParagraphStyle> style() const;
  };

  class Text final : public Extension<internal::abstract::TextElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string value() const;

    [[nodiscard]] std::optional<TextStyle> style() const;
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

    [[nodiscard]] std::optional<TableStyle> style() const;
  };

  class TableColumn final
      : public Extension<internal::abstract::TableColumnElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<TableColumnStyle> style() const;
  };

  class TableRow final : public Extension<internal::abstract::TableRowElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<TableRowStyle> style() const;
  };

  class TableCell final
      : public Extension<internal::abstract::TableCellElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] TableDimensions span() const;

    [[nodiscard]] std::optional<TableCellStyle> style() const;
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

    [[nodiscard]] std::optional<GraphicStyle> style() const;
  };

  class Line final : public Extension<internal::abstract::LineElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string x1() const;
    [[nodiscard]] std::string y1() const;
    [[nodiscard]] std::string x2() const;
    [[nodiscard]] std::string y2() const;

    [[nodiscard]] std::optional<GraphicStyle> style() const;
  };

  class Circle final : public Extension<internal::abstract::CircleElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string x() const;
    [[nodiscard]] std::string y() const;
    [[nodiscard]] std::string width() const;
    [[nodiscard]] std::string height() const;

    [[nodiscard]] std::optional<GraphicStyle> style() const;
  };

  class CustomShape final
      : public Extension<internal::abstract::CustomShapeElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<std::string> x() const;
    [[nodiscard]] std::optional<std::string> y() const;
    [[nodiscard]] std::string width() const;
    [[nodiscard]] std::string height() const;

    [[nodiscard]] std::optional<GraphicStyle> style() const;
  };

  class Image final : public Extension<internal::abstract::ImageElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] bool internal() const;
    [[nodiscard]] std::optional<odr::File> file() const;
    [[nodiscard]] std::string href() const;
  };

  [[nodiscard]] TextRoot text_root() const;
  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
  [[nodiscard]] Paragraph paragraph() const;
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
  const internal::abstract::DocumentCursor *m_cursor;
  internal::abstract::Element *m_element;
};

template <typename T> struct DirectionalStyle final {
  std::optional<T> right;
  std::optional<T> top;
  std::optional<T> left;
  std::optional<T> bottom;

  DirectionalStyle() = default;
  DirectionalStyle(std::optional<T> all)
      : right{all}, top{all}, left{all}, bottom{all} {}

  void override(const DirectionalStyle &other) {
    if (other.right) {
      right = other.right;
    }
    if (other.top) {
      top = other.top;
    }
    if (other.left) {
      left = other.left;
    }
    if (other.bottom) {
      bottom = other.bottom;
    }
  }
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

  void override(const TextStyle &other);
};

struct ParagraphStyle final {
  std::optional<std::string> text_align;
  DirectionalStyle<std::string> margin;

  void override(const ParagraphStyle &other);
};

struct TableStyle final {
  std::optional<std::string> width;

  void override(const TableStyle &other);
};

struct TableColumnStyle final {
  std::optional<std::string> width;

  void override(const TableColumnStyle &other);
};

struct TableRowStyle final {
  std::optional<std::string> height;

  void override(const TableRowStyle &other);
};

struct TableCellStyle final {
  std::optional<std::string> vertical_align;
  std::optional<std::string> background_color;
  DirectionalStyle<std::string> padding;
  DirectionalStyle<std::string> border;

  void override(const TableCellStyle &other);
};

struct GraphicStyle final {
  std::optional<std::string> stroke_width;
  std::optional<std::string> stroke_color;
  std::optional<std::string> fill_color;
  std::optional<std::string> vertical_align;

  void override(const GraphicStyle &other);
};

struct PageLayout final {
  std::optional<std::string> width;
  std::optional<std::string> height;
  std::optional<std::string> print_orientation;
  DirectionalStyle<std::string> margin;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

} // namespace odr

#endif // ODR_DOCUMENT_H
