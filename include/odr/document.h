#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace odr::internal::abstract {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

class DocumentCursor;

class Element;
class TextRootElement;
class SlideElement;
class SheetElement;
class PageElement;
class LineBreakElement;
class ParagraphElement;
class SpanElement;
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
struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct PageLayout;
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

class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;
class DocumentCursor;
class Element;
class TextRoot;
class Slide;
class Sheet;
class Page;
class LineBreak;
class Paragraph;
class Span;
class Text;
class Link;
class Bookmark;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
class Frame;
class Rect;
class Line;
class Circle;
class CustomShape;
class Image;

class Document {
public:
  explicit Document(std::shared_ptr<internal::abstract::Document> document);

  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] DocumentCursor root_element() const;

  [[nodiscard]] TextDocument text_document() const;
  [[nodiscard]] Presentation presentation() const;
  [[nodiscard]] Spreadsheet spreadsheet() const;
  [[nodiscard]] Drawing drawing() const;

private:
  std::shared_ptr<internal::abstract::Document> m_document;

  friend DocumentFile;
};

template <typename T> class TypedDocument : public Document {
public:
  explicit TypedDocument(std::shared_ptr<T> document)
      : Document(document), m_document{std::move(m_document)} {}
  explicit TypedDocument(std::shared_ptr<internal::abstract::Document> document)
      : Document(document), m_document{std::dynamic_pointer_cast<T>(
                                std::move(document))} {}

  explicit operator bool() const { return m_document; }

protected:
  std::shared_ptr<T> m_document;
};

class TextDocument final
    : public TypedDocument<internal::abstract::TextDocument> {
public:
  using TypedDocument::TypedDocument;

  [[nodiscard]] PageLayout page_layout() const;
};

class Presentation final
    : public TypedDocument<internal::abstract::Presentation> {
public:
  using TypedDocument::TypedDocument;

  [[nodiscard]] std::uint32_t slide_count() const;
};

class Spreadsheet final
    : public TypedDocument<internal::abstract::Spreadsheet> {
public:
  using TypedDocument::TypedDocument;

  [[nodiscard]] std::uint32_t sheet_count() const;
};

class Drawing final : public TypedDocument<internal::abstract::Drawing> {
public:
  using TypedDocument::TypedDocument;

  [[nodiscard]] std::uint32_t page_count() const;
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

class Element {
public:
  Element() = default;
  Element(const internal::abstract::Document *document,
          const internal::abstract::DocumentCursor *cursor,
          internal::abstract::Element *element);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TextRoot text_root() const;
  [[nodiscard]] Slide slide() const;
  [[nodiscard]] Sheet sheet() const;
  [[nodiscard]] Page page() const;
  [[nodiscard]] LineBreak line_break() const;
  [[nodiscard]] Paragraph paragraph() const;
  [[nodiscard]] Span span() const;
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

protected:
  const internal::abstract::Document *m_document{nullptr};
  const internal::abstract::DocumentCursor *m_cursor{nullptr};
  internal::abstract::Element *m_element{nullptr};
};

template <typename T> class TypedElement : public Element {
public:
  TypedElement() = default;
  TypedElement(const internal::abstract::Document *document,
               const internal::abstract::DocumentCursor *cursor, T *element)
      : Element(document, cursor, element), m_element{element} {}
  TypedElement(const internal::abstract::Document *document,
               const internal::abstract::DocumentCursor *cursor,
               internal::abstract::Element *element)
      : Element(document, cursor, element), m_element{
                                                dynamic_cast<T *>(element)} {}

protected:
  T *m_element{nullptr};
};

class TextRoot final
    : public TypedElement<internal::abstract::TextRootElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] PageLayout page_layout() const;
};

class Slide final : public TypedElement<internal::abstract::SlideElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] PageLayout page_layout() const;
};

class Sheet final : public TypedElement<internal::abstract::SheetElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;

  [[nodiscard]] TableDimensions
  content(std::optional<TableDimensions> range) const;
};

class Page final : public TypedElement<internal::abstract::PageElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] PageLayout page_layout() const;
};

class LineBreak final
    : public TypedElement<internal::abstract::LineBreakElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<TextStyle> style() const;
};

class Paragraph final
    : public TypedElement<internal::abstract::ParagraphElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<ParagraphStyle> style() const;
  [[nodiscard]] std::optional<TextStyle> text_style() const;
};

class Span final : public TypedElement<internal::abstract::SpanElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<TextStyle> style() const;
};

class Text final : public TypedElement<internal::abstract::TextElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string content() const;
  void set_content(const std::string &text) const;

  [[nodiscard]] std::optional<TextStyle> style() const;
};

class Link final : public TypedElement<internal::abstract::LinkElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string href() const;
};

class Bookmark final
    : public TypedElement<internal::abstract::BookmarkElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string name() const;
};

class ListItem final
    : public TypedElement<internal::abstract::ListItemElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<TextStyle> style() const;
};

class Table final : public TypedElement<internal::abstract::TableElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] std::optional<TableStyle> style() const;
};

class TableColumn final
    : public TypedElement<internal::abstract::TableColumnElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<TableColumnStyle> style() const;
};

class TableRow final
    : public TypedElement<internal::abstract::TableRowElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<TableRowStyle> style() const;
};

class TableCell final
    : public TypedElement<internal::abstract::TableCellElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] TableColumn column() const;
  [[nodiscard]] TableRow row() const;

  [[nodiscard]] bool covered() const;
  [[nodiscard]] TableDimensions span() const;
  [[nodiscard]] ValueType value_type() const;

  [[nodiscard]] std::optional<TableCellStyle> style() const;
};

class Frame final : public TypedElement<internal::abstract::FrameElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] AnchorType anchor_type() const;
  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::optional<std::string> width() const;
  [[nodiscard]] std::optional<std::string> height() const;
  [[nodiscard]] std::optional<std::string> z_index() const;

  [[nodiscard]] std::optional<GraphicStyle> style() const;
};

class Rect final : public TypedElement<internal::abstract::RectElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] std::optional<GraphicStyle> style() const;
};

class Line final : public TypedElement<internal::abstract::LineElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] std::optional<GraphicStyle> style() const;
};

class Circle final : public TypedElement<internal::abstract::CircleElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] std::optional<GraphicStyle> style() const;
};

class CustomShape final
    : public TypedElement<internal::abstract::CustomShapeElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] std::optional<std::string> x() const;
  [[nodiscard]] std::optional<std::string> y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] std::optional<GraphicStyle> style() const;
};

class Image final : public TypedElement<internal::abstract::ImageElement> {
public:
  using TypedElement::TypedElement;

  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::optional<odr::File> file() const;
  [[nodiscard]] std::string href() const;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
