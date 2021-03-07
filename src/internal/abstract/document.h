#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace odr::experimental {
enum class DocumentType;
struct DocumentMeta;
enum class ElementType;
struct TableDimensions;
} // namespace odr::experimental

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {
class File;
class DocumentFile;

class Element;
class Slide;
class Sheet;
class Page;
class PageStyle;
class ParagraphStyle;
class TextStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;
class Property;

class Document {
public:
  virtual ~Document() = default;

  [[nodiscard]] virtual bool editable() const noexcept = 0;
  [[nodiscard]] virtual bool savable(bool encrypted = false) const noexcept = 0;

  [[nodiscard]] virtual experimental::DocumentType
  document_type() const noexcept = 0;
  [[nodiscard]] virtual experimental::DocumentMeta
  document_meta() const noexcept = 0;

  [[nodiscard]] virtual std::shared_ptr<const Element> root() const = 0;

  virtual void save(const common::Path &path) const = 0;
  virtual void save(const common::Path &path,
                    const std::string &password) const = 0;
};

class TextDocument : public virtual Document {
public:
  [[nodiscard]] experimental::DocumentType document_type() const noexcept final;

  [[nodiscard]] virtual std::shared_ptr<PageStyle> page_style() const = 0;
};

class Presentation : public virtual Document {
public:
  [[nodiscard]] experimental::DocumentType document_type() const noexcept final;

  [[nodiscard]] virtual std::uint32_t slide_count() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Slide> first_slide() const = 0;
};

class Spreadsheet : public virtual Document {
public:
  [[nodiscard]] experimental::DocumentType document_type() const noexcept final;

  [[nodiscard]] virtual std::uint32_t sheet_count() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Sheet> first_sheet() const = 0;
};

class Drawing : public virtual Document {
public:
  [[nodiscard]] experimental::DocumentType document_type() const noexcept final;

  [[nodiscard]] virtual std::uint32_t page_count() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const Page> first_page() const = 0;
};

class ElementIterator {
  virtual ~ElementIterator() = default;

  [[nodiscard]] virtual std::unique_ptr<ElementIterator> clone() const = 0;

  [[nodiscard]] virtual experimental::ElementType type() const = 0;

  // valid for type = SLIDE, SHEET, PAGE, BOOKMARK
  [[nodiscard]] virtual std::string name() const = 0;
  // valid for type = SLIDE
  [[nodiscard]] virtual std::string notes() const = 0;
  // valid for type = TEXT
  [[nodiscard]] virtual std::string text() const = 0;
  // valid for type = TABLE
  [[nodiscard]] virtual experimental::TableDimensions
  table_dimensions() const = 0;
  // valid for type = TABLE_CELL
  [[nodiscard]] virtual std::uint32_t row_span() const = 0;
  // valid for type = TABLE_CELL
  [[nodiscard]] virtual std::uint32_t column_span() const = 0;
  // valid for type = FRAME
  [[nodiscard]] virtual std::shared_ptr<Property> anchor_type() const = 0;
  // valid for type = RECT, CIRCLE
  [[nodiscard]] virtual std::string x() const = 0;
  // valid for type = RECT, CIRCLE
  [[nodiscard]] virtual std::string y() const = 0;
  // valid for type = FRAME, RECT, CIRCLE
  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
  // valid for type = FRAME, RECT, CIRCLE
  [[nodiscard]] virtual std::shared_ptr<Property> height() const = 0;
  // valid for type = FRAME
  [[nodiscard]] virtual std::shared_ptr<Property> z_index() const = 0;
  // valid for type = IMAGE
  [[nodiscard]] virtual bool internal() const = 0;
  // valid for type = LINK, IMAGE
  [[nodiscard]] virtual std::string href() const = 0;
  // valid for type = IMAGE
  [[nodiscard]] virtual std::shared_ptr<File> image_file() const = 0;
  // valid for type = LINE
  [[nodiscard]] virtual std::string x1() const = 0;
  // valid for type = LINE
  [[nodiscard]] virtual std::string y1() const = 0;
  // valid for type = LINE
  [[nodiscard]] virtual std::string x2() const = 0;
  // valid for type = LINE
  [[nodiscard]] virtual std::string y2() const = 0;

  // valid for type = SLIDE, PAGE
  [[nodiscard]] virtual std::shared_ptr<PageStyle> page_style() const = 0;
  // valid for type = PARAGRAPH
  [[nodiscard]] virtual std::shared_ptr<ParagraphStyle>
  paragraph_style() const = 0;
  // valid for type = PARAGRAPH, SPAN, LINK
  [[nodiscard]] virtual std::shared_ptr<TextStyle> text_style() const = 0;
  // valid for type = TABLE
  [[nodiscard]] virtual std::shared_ptr<TableStyle> table_style() const = 0;
  // valid for type = TABLE_COLUMN
  [[nodiscard]] virtual std::shared_ptr<TableColumnStyle>
  table_column_style() const = 0;
  // valid for type = TABLE_CELL
  [[nodiscard]] virtual std::shared_ptr<TableCellStyle>
  table_cell_style() const = 0;
  // valid for type = RECT, LINE, CIRCLE
  [[nodiscard]] virtual std::shared_ptr<DrawingStyle> drawing_style() const = 0;

  virtual bool parent() = 0;
  virtual bool first_child() = 0;
  virtual bool previous_sibling() = 0;
  virtual bool next_sibling() = 0;

  // valid for type = SHEET
  virtual bool table() = 0;
  // valid for type = TABLE
  virtual bool first_column() = 0;
  // valid for type = TABLE
  virtual bool first_row() = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
