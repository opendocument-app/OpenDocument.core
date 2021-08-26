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
class TableElement;
class ImageElement;
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
  ROW_SPAN,
  COLUMN_SPAN,
  ROWS_REPEATED,
  COLUMNS_REPEATED,

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

class DocumentCursor;
class ElementPropertySet;
class ImageElement;

class Document final {
public:
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] DocumentCursor root_element() const;

private:
  std::shared_ptr<internal::abstract::Document> m_impl;

  explicit Document(std::shared_ptr<internal::abstract::Document> impl);

  friend DocumentFile;
};

class DocumentCursor final {
public:
  bool operator==(const DocumentCursor &rhs) const;
  bool operator!=(const DocumentCursor &rhs) const;

  [[nodiscard]] std::string document_path() const;

  [[nodiscard]] ElementType element_type() const;
  [[nodiscard]] ElementPropertySet element_properties() const;

  ImageElement image() const;

  bool move_to_parent();
  bool move_to_first_child();
  bool move_to_previous_sibling();
  bool move_to_next_sibling();

  using ChildVisitor =
      std::function<void(DocumentCursor &cursor, std::uint32_t i)>;

  void for_each_child(ChildVisitor visitor);

private:
  std::shared_ptr<internal::abstract::DocumentCursor> m_impl;

  explicit DocumentCursor(
      std::shared_ptr<internal::abstract::DocumentCursor> impl);

  friend Document;
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

class ImageElement final {
public:
  explicit operator bool() const;

  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::optional<File> image_file() const;

private:
  explicit ImageElement(internal::abstract::ImageElement *impl);

  internal::abstract::ImageElement *m_impl;

  friend DocumentCursor;
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
