#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace odr {
enum class DocumentType;
enum class ElementType;
enum class StyleContext;
class TableDimensions;
class File;
} // namespace odr

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {
class ReadableFilesystem;

class DocumentCursor;

class Element;

class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableRowStyle;
class TableCellStyle;
class GraphicStyle;
class PageLayout;

class Property;
class DirectionalProperty;

using Allocator = std::function<void *(std::size_t size)>;

class Document {
public:
  virtual ~Document() = default;

  /// \return `true` if the document is editable in any way.
  [[nodiscard]] virtual bool editable() const noexcept = 0;

  /// \param encrypted to ask for encrypted saves.
  /// \return `true` if the document is savable.
  [[nodiscard]] virtual bool savable(bool encrypted) const noexcept = 0;

  /// \param path the destination path.
  virtual void save(const common::Path &path) const = 0;

  /// \param path the destination path.
  /// \param password the encryption password.
  virtual void save(const common::Path &path, const char *password) const = 0;

  /// \return the type of the document.
  [[nodiscard]] virtual DocumentType document_type() const noexcept = 0;

  [[nodiscard]] virtual std::shared_ptr<ReadableFilesystem>
  files() const noexcept = 0;

  /// \return cursor to the root element of the document.
  [[nodiscard]] virtual std::unique_ptr<DocumentCursor>
  root_element() const = 0;
};

class DocumentCursor {
public:
  virtual ~DocumentCursor() = default;

  [[nodiscard]] virtual bool equals(const DocumentCursor &other) const = 0;

  [[nodiscard]] virtual std::unique_ptr<DocumentCursor> copy() const = 0;

  [[nodiscard]] virtual std::string document_path() const = 0;

  [[nodiscard]] virtual Element *element() = 0;
  [[nodiscard]] virtual const Element *element() const = 0;

  virtual bool move_to_parent() = 0;
  virtual bool move_to_first_child() = 0;
  virtual bool move_to_previous_sibling() = 0;
  virtual bool move_to_next_sibling() = 0;

  virtual bool move_to_master_page() = 0;

  virtual bool move_to_first_table_column() = 0;
  virtual bool move_to_first_table_row() = 0;
};

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual bool equals(const Document *document,
                                    const Element &other) const = 0;

  [[nodiscard]] virtual ElementType type(const Document *document) const = 0;

  virtual Element *parent(const Document *document,
                          const Allocator &allocator) = 0;
  virtual Element *first_child(const Document *document,
                               const Allocator &allocator) = 0;
  virtual Element *previous_sibling(const Document *document,
                                    const Allocator &allocator) = 0;
  virtual Element *next_sibling(const Document *document,
                                const Allocator &allocator) = 0;
};

class TextRootElement : public virtual Element {
public:
  [[nodiscard]] virtual PageLayout *
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  first_master_page(const Document *document,
                    const Allocator &allocator) const = 0;
};

class SlideElement : public virtual Element {
public:
  [[nodiscard]] virtual PageLayout *
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document, const Allocator &allocator) const = 0;

  [[nodiscard]] virtual std::string *name(const Document *document) const = 0;
};

class SheetElement : public virtual Element {
public:
  [[nodiscard]] virtual std::string *name(const Document *document) const = 0;
};

class PageElement : public virtual Element {
public:
  [[nodiscard]] virtual PageLayout *
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document, const Allocator &allocator) const = 0;

  [[nodiscard]] virtual std::string *name(const Document *document) const = 0;
};

class MasterPageElement : public virtual Element {
public:
  [[nodiscard]] virtual PageLayout *
  page_layout(const Document *document) const = 0;
};

class ParagraphElement : public virtual Element {
public:
  [[nodiscard]] virtual TextStyle *
  text_style(const Document *document) const = 0;
  [[nodiscard]] virtual ParagraphStyle *
  paragraph_style(const Document *document) const = 0;
};

class SpanElement : public virtual Element {
public:
  [[nodiscard]] virtual TextStyle *
  text_style(const Document *document) const = 0;
};

class TextElement : public virtual Element {
public:
  [[nodiscard]] virtual std::string value(const Document *document) const = 0;
};

class LinkElement : public virtual Element {
public:
  [[nodiscard]] virtual TextStyle *
  text_style(const Document *document) const = 0;

  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

class BookmarkElement : public virtual Element {
public:
  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class TableElement : public virtual Element {
public:
  [[nodiscard]] virtual TableStyle *
  table_style(const Document *document) const = 0;

  [[nodiscard]] virtual TableDimensions
  dimensions(const Document *document) const = 0;
  [[nodiscard]] virtual Element *
  first_column(const Document *document, const Allocator &allocator) const = 0;
  [[nodiscard]] virtual Element *
  first_row(const Document *document, const Allocator &allocator) const = 0;
};

class TableColumnElement : public virtual Element {
public:
  [[nodiscard]] virtual TableColumnStyle *
  table_column_style(const Document *document) const = 0;
};

class TableRowElement : public virtual Element {
public:
  [[nodiscard]] virtual TableRowStyle *
  table_row_style(const Document *document) const = 0;
};

class TableCellElement : public virtual Element {
public:
  [[nodiscard]] virtual TableCellStyle *
  table_cell_style(const Document *document) const = 0;

  [[nodiscard]] virtual TableDimensions
  span(const Document *document) const = 0;
};

class FrameElement : public virtual Element {
public:
  [[nodiscard]] virtual std::optional<std::string>
  anchor_type(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  width(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  height(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  z_index(const Document *document) const = 0;
};

class RectElement : public virtual Element {
public:
  [[nodiscard]] virtual GraphicStyle *
  graphic_style(const Document *document) const = 0;

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;
};

class LineElement : public virtual Element {
public:
  [[nodiscard]] virtual GraphicStyle *
  graphic_style(const Document *document) const = 0;

  [[nodiscard]] virtual std::string x1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string x2(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y2(const Document *document) const = 0;
};

class CircleElement : public virtual Element {
public:
  [[nodiscard]] virtual GraphicStyle *
  graphic_style(const Document *document) const = 0;

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;
};

class CustomShapeElement : public virtual Element {
public:
  [[nodiscard]] virtual GraphicStyle *
  graphic_style(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;
};

class ImageElement : public virtual Element {
public:
  [[nodiscard]] virtual bool internal(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  file(const Document *document) const = 0;
  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

class Style {
public:
  virtual ~Style() = default;

  virtual std::optional<std::string> name() const = 0;
  virtual const Style *parent() const = 0;
};

class TextStyle : public virtual Style {
public:
  virtual const TextStyle *parent() const = 0;

  [[nodiscard]] virtual Property *font_name(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_size(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_weight(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_style(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_underline(const Document *document) = 0;
  [[nodiscard]] virtual Property *
  font_line_through(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_shadow(const Document *document) = 0;
  [[nodiscard]] virtual Property *font_color(const Document *document) = 0;
  [[nodiscard]] virtual Property *
  background_color(const Document *document) = 0;
};

class ParagraphStyle : public virtual Style {
public:
  virtual const ParagraphStyle *parent() const = 0;

  [[nodiscard]] virtual Property *text_align(const Document *document) = 0;
  [[nodiscard]] virtual DirectionalProperty *
  margin(const Document *document) = 0;
};

class TableStyle : public virtual Style {
public:
  [[nodiscard]] virtual Property *width(const Document *document) = 0;
};

class TableColumnStyle : public virtual Style {
public:
  [[nodiscard]] virtual Property *width(const Document *document) = 0;
};

class TableRowStyle : public virtual Style {
public:
  [[nodiscard]] virtual Property *height(const Document *document) = 0;
};

class TableCellStyle : public virtual Style {
public:
  [[nodiscard]] virtual Property *vertical_align(const Document *document) = 0;
  [[nodiscard]] virtual Property *
  background_color(const Document *document) = 0;
  [[nodiscard]] virtual DirectionalProperty *
  padding(const Document *document) = 0;
  [[nodiscard]] virtual DirectionalProperty *
  border(const Document *document) = 0;
};

class GraphicStyle : public virtual Style {
public:
  virtual const GraphicStyle *parent() const = 0;

  [[nodiscard]] virtual Property *stroke_width(const Document *document) = 0;
  [[nodiscard]] virtual Property *stroke_color(const Document *document) = 0;
  [[nodiscard]] virtual Property *fill_color(const Document *document) = 0;
  [[nodiscard]] virtual Property *vertical_align(const Document *document) = 0;
};

class PageLayout : public virtual Style {
public:
  [[nodiscard]] virtual Property *width(const Document *document) = 0;
  [[nodiscard]] virtual Property *height(const Document *document) = 0;
  [[nodiscard]] virtual Property *
  print_orientation(const Document *document) = 0;
  [[nodiscard]] virtual DirectionalProperty *
  margin(const Document *document) = 0;
};

class Property {
public:
  virtual ~Property() = default;

  [[nodiscard]] virtual std::optional<std::string>
  value(const Document *document, const Element *element, const Style *style,
        StyleContext style_context) const = 0;
};

class DirectionalProperty {
public:
  virtual ~DirectionalProperty() = default;

  [[nodiscard]] virtual Property *right(const Document *document) = 0;
  [[nodiscard]] virtual Property *top(const Document *document) = 0;
  [[nodiscard]] virtual Property *left(const Document *document) = 0;
  [[nodiscard]] virtual Property *bottom(const Document *document) = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
