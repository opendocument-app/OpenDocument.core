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

template <typename Number, typename Tag> struct Identifier {
  Identifier() = default;
  Identifier(Number id) : id{id} {}

  operator bool() const { return id == 0; }
  operator Number() const { return id; }

  Number id{0};
};

struct element_identifier_tag {};

using ElementIdentifier = Identifier<std::uint64_t, element_identifier_tag>;

class Elements {
public:
  virtual ~Elements() = default;

  [[nodiscard]] virtual ElementIdentifier
  parent(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  first_child(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  previous_sibling(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  next_sibling(ElementIdentifier elementId) const = 0;

  [[nodiscard]] virtual experimental::ElementType
  type(ElementIdentifier elementId) const = 0;

  // slide
  [[nodiscard]] virtual std::string
  slide_name(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  slide_notes(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<PageStyle>
  slide_style(ElementIdentifier elementId) const = 0;

  // sheet
  [[nodiscard]] virtual std::string
  sheet_name(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  sheet_table(ElementIdentifier elementId) const = 0;

  // page
  [[nodiscard]] virtual std::string
  page_name(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<PageStyle>
  page_style(ElementIdentifier elementId) const = 0;

  // text
  [[nodiscard]] virtual std::string text(ElementIdentifier elementId) const = 0;

  // paragraph
  [[nodiscard]] virtual std::shared_ptr<ParagraphStyle>
  paragraph_style(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<TextStyle>
  paragraph_text_style(ElementIdentifier elementId) const = 0;

  // span
  [[nodiscard]] virtual std::shared_ptr<TextStyle>
  span_text_style(ElementIdentifier elementId) const = 0;

  // bookmark
  [[nodiscard]] virtual std::string
  bookmark_name(ElementIdentifier elementId) const = 0;

  // link
  [[nodiscard]] virtual std::string
  link_href(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<TextStyle>
  link_text_style(ElementIdentifier elementId) const = 0;

  // table
  [[nodiscard]] virtual ElementIdentifier
  first_column(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  first_row(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<TableStyle>
  table_style(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual experimental::TableDimensions
  table_dimensions(ElementIdentifier elementId) const = 0;

  // table column
  [[nodiscard]] virtual std::shared_ptr<TableColumnStyle>
  table_column_style(ElementIdentifier elementId) const = 0;

  // table cell
  [[nodiscard]] virtual std::uint32_t
  table_cell_row_span(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::uint32_t
  table_cell_column_span(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<TableCellStyle>
  table_cell_style(ElementIdentifier elementId) const = 0;

  // frame
  [[nodiscard]] virtual std::shared_ptr<Property>
  frame_anchor_type(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property>
  frame_width(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property>
  frame_height(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property>
  frame_z_index(ElementIdentifier elementId) const = 0;

  // image
  [[nodiscard]] virtual bool
  image_internal(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  image_href(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<File>
  image_file(ElementIdentifier elementId) const = 0;

  // rect
  [[nodiscard]] virtual std::string
  rect_x(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  rect_y(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<DrawingStyle>
  rect_style(ElementIdentifier elementId) const = 0;

  // circle
  [[nodiscard]] virtual std::string
  circle_x(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  circle_y(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<DrawingStyle>
  circle_style(ElementIdentifier elementId) const = 0;

  // line
  [[nodiscard]] virtual std::string
  line_x1(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  line_y1(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  line_x2(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::string
  line_y2(ElementIdentifier elementId) const = 0;
  [[nodiscard]] virtual std::shared_ptr<DrawingStyle>
  line_style(ElementIdentifier elementId) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
