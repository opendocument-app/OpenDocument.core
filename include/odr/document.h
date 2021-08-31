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
} // namespace odr::internal::abstract

namespace odr {

enum class DocumentType;
class File;
class DocumentFile;

enum class ElementType {
  NONE,

  ROOT,
  SLIDE,
  SHEET,
  PAGE,

  MASTER_PAGE,

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

  GROUP,
};

// TODO the property handle could reflect the type
// TODO it might be possible to strongly type these properties
enum class ElementProperty {
  NONE,

  NAME,
  NOTES,
  HREF,
  ANCHOR_TYPE,

  STYLE_NAME,
  MASTER_PAGE_NAME,

  VALUE_TYPE,

  PLACEHOLDER,

  X,
  Y,
  WIDTH,
  HEIGHT,

  X1,
  Y1,
  X2,
  Y2,

  Z_INDEX,

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

struct TableDimensions;
class DocumentCursor;
class ElementPropertySet;

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
  [[nodiscard]] ElementPropertySet element_properties() const;

  bool move_to_parent();
  bool move_to_first_child();
  bool move_to_previous_sibling();
  bool move_to_next_sibling();

  [[nodiscard]] std::string text() const;

  [[nodiscard]] bool move_to_master_page();

  [[nodiscard]] TableDimensions table_dimensions();
  [[nodiscard]] bool move_to_first_table_column();
  [[nodiscard]] bool move_to_first_table_row();
  [[nodiscard]] TableDimensions table_cell_span();

  [[nodiscard]] bool image_internal() const;
  [[nodiscard]] std::optional<File> image_file() const;

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

  explicit DocumentCursor(
      std::shared_ptr<internal::abstract::Document> document,
      std::shared_ptr<internal::abstract::DocumentCursor> cursor);

  void for_each_(const ChildVisitor &visitor);
  void for_each_(const ConditionalChildVisitor &visitor);

  friend Document;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

class ElementPropertySet final {
public:
  [[nodiscard]] std::any get(ElementProperty property) const;
  [[nodiscard]] std::optional<std::string>
  get_string(ElementProperty property) const;
  [[nodiscard]] std::optional<std::uint32_t>
  get_uint32(ElementProperty property) const;
  [[nodiscard]] std::optional<bool> get_bool(ElementProperty property) const;

private:
  explicit ElementPropertySet(
      std::unordered_map<ElementProperty, std::any> properties);

  std::unordered_map<ElementProperty, std::any> m_properties;

  friend DocumentCursor;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
