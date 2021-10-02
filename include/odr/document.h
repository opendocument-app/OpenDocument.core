#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <functional>
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
class ParagraphElement;
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
  DocumentCursor(const DocumentCursor &other);
  ~DocumentCursor();
  DocumentCursor &operator=(const DocumentCursor &other);

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

  [[nodiscard]] bool move_to_first_sheet_shape();

  void move(const std::string &path);

  using ChildVisitor =
      std::function<void(DocumentCursor &cursor, std::uint32_t i)>;
  using ConditionalChildVisitor =
      std::function<bool(DocumentCursor &cursor, std::uint32_t i)>;

  void for_each_child(const ChildVisitor &visitor);
  void for_each_table_column(const ConditionalChildVisitor &visitor);
  void for_each_table_row(const ConditionalChildVisitor &visitor);
  void for_each_table_cell(const ConditionalChildVisitor &visitor);
  void for_each_sheet_shape(const ChildVisitor &visitor);

private:
  std::shared_ptr<internal::abstract::Document> m_document;
  std::unique_ptr<internal::abstract::DocumentCursor> m_cursor;

  DocumentCursor(std::shared_ptr<internal::abstract::Document> document,
                 std::unique_ptr<internal::abstract::DocumentCursor> cursor);

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
    Extension() = default;
    Extension(const internal::abstract::Document *document,
              const internal::abstract::DocumentCursor *cursor, T *element)
        : m_document{document}, m_cursor{cursor}, m_element{element} {}
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

    [[nodiscard]] TableDimensions
    content(std::optional<TableDimensions> range) const;
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
    [[nodiscard]] std::optional<TextStyle> text_style() const;
  };

  class Text final : public Extension<internal::abstract::TextElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::string content() const;
    void set_content(const std::string &text) const;

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

  class ListItem final : public Extension<internal::abstract::ListItemElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] std::optional<TextStyle> style() const;
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

    [[nodiscard]] TableColumn column() const;
    [[nodiscard]] TableRow row() const;

    [[nodiscard]] bool covered() const;
    [[nodiscard]] TableDimensions span() const;
    [[nodiscard]] ValueType value_type() const;

    [[nodiscard]] std::optional<TableCellStyle> style() const;
  };

  class Frame final : public Extension<internal::abstract::FrameElement> {
  public:
    using Extension::Extension;

    [[nodiscard]] AnchorType anchor_type() const;
    [[nodiscard]] std::optional<std::string> x() const;
    [[nodiscard]] std::optional<std::string> y() const;
    [[nodiscard]] std::optional<std::string> width() const;
    [[nodiscard]] std::optional<std::string> height() const;
    [[nodiscard]] std::optional<std::string> z_index() const;

    [[nodiscard]] std::optional<GraphicStyle> style() const;
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

private:
  const internal::abstract::Document *m_document;
  const internal::abstract::DocumentCursor *m_cursor;
  internal::abstract::Element *m_element;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
