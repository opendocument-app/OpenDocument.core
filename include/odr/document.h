#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <odr/document_elements.h>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr {
enum class DocumentType;
struct DocumentMeta;
class DocumentFile;
class PageStyle;

class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

class Document {
public:
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] std::uint32_t entry_count() const;

  [[nodiscard]] Element root() const;

  [[nodiscard]] Element first_entry() const;

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
};

// TODO the property handle could reflect the type
// TODO it might be possible to strongly type these properties
enum class ElementProperty {
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
};

class ElementPropertyValue {
public:
  bool operator==(const ElementPropertyValue &rhs) const;
  bool operator!=(const ElementPropertyValue &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] std::any get() const;
  [[nodiscard]] std::string get_string() const;
  [[nodiscard]] std::uint32_t get_uint32() const;
  [[nodiscard]] bool get_bool() const;
  [[nodiscard]] const char *get_optional_string() const;

  void set(const std::any &value) const;
  void set_string(const std::string &value) const;

  void remove() const;

private:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};
  ElementProperty m_property;

  ElementPropertyValue();
  ElementPropertyValue(std::shared_ptr<const internal::abstract::Document> impl,
                       std::uint64_t id, ElementProperty property);

  friend Element;
  friend PageStyle;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
