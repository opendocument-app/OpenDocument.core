#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <functional>
#include <memory>
#include <odr/document.h>
#include <optional>
#include <string>

namespace odr {
class File;
enum class DocumentType;
enum class ElementType;
struct TextStyle;
struct ParagraphStyle;
struct TableStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableCellStyle;
struct GraphicStyle;
struct PageLayout;
struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
class Path;
class DocumentPath;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class ReadableFilesystem;

class DocumentCursor;
class Element;

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

  [[nodiscard]] virtual common::DocumentPath document_path() const = 0;

  [[nodiscard]] virtual Element *element() = 0;
  [[nodiscard]] virtual const Element *element() const = 0;

  virtual bool move_to_parent() = 0;
  virtual bool move_to_first_child() = 0;
  virtual bool move_to_previous_sibling() = 0;
  virtual bool move_to_next_sibling() = 0;

  virtual bool move_to_master_page() = 0;

  virtual bool move_to_first_table_column() = 0;
  virtual bool move_to_first_table_row() = 0;

  virtual bool move_to_first_sheet_shape() = 0;

  virtual void move(const common::DocumentPath &path) = 0;
};

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual bool equals(const Document *document,
                                    const DocumentCursor *cursor,
                                    const Element &other) const = 0;

  [[nodiscard]] virtual ElementType type(const Document *document) const = 0;

  virtual Element *parent(const Document *document,
                          const DocumentCursor *cursor,
                          const Allocator *allocator) = 0;
  virtual Element *first_child(const Document *document,
                               const DocumentCursor *cursor,
                               const Allocator *allocator) = 0;
  virtual Element *previous_sibling(const Document *document,
                                    const DocumentCursor *cursor,
                                    const Allocator *allocator) = 0;
  virtual Element *next_sibling(const Document *document,
                                const DocumentCursor *cursor,
                                const Allocator *allocator) = 0;
};

class TextRootElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::root;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  first_master_page(const Document *document, const DocumentCursor *cursor,
                    const Allocator *allocator) const = 0;
};

class SlideElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::slide;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document, const DocumentCursor *cursor,
              const Allocator *allocator) const = 0;

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class SheetElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::sheet;
  }

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;

  [[nodiscard]] virtual TableDimensions
  content(const Document *document,
          std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual Element *
  first_shape(const Document *document, const DocumentCursor *cursor,
              const Allocator *allocator) const = 0;
};

class PageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::page;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;

  [[nodiscard]] virtual Element *
  master_page(const Document *document, const DocumentCursor *cursor,
              const Allocator *allocator) const = 0;

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class MasterPageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::master_page;
  }

  [[nodiscard]] virtual PageLayout
  page_layout(const Document *document) const = 0;
};

class ParagraphElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::paragraph;
  }

  [[nodiscard]] virtual std::optional<ParagraphStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class SpanElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::span;
  }
};

class TextElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::text;
  }

  [[nodiscard]] virtual std::string content(const Document *document) const = 0;
  virtual void set_content(const Document *document,
                           const std::string &text) = 0;

  [[nodiscard]] virtual std::optional<TextStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class LinkElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::link;
  }

  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

class BookmarkElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::bookmark;
  }

  [[nodiscard]] virtual std::string name(const Document *document) const = 0;
};

class ListItemElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::list_item;
  }

  [[nodiscard]] virtual std::optional<TextStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class TableElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table;
  }

  [[nodiscard]] virtual TableDimensions
  dimensions(const Document *document) const = 0;
  [[nodiscard]] virtual Element *
  first_column(const Document *document, const DocumentCursor *cursor,
               const Allocator *allocator) const = 0;
  [[nodiscard]] virtual Element *
  first_row(const Document *document, const DocumentCursor *cursor,
            const Allocator *allocator) const = 0;

  [[nodiscard]] virtual std::optional<TableStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class TableColumnElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_column;
  }

  [[nodiscard]] virtual std::optional<TableColumnStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class TableRowElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_row;
  }

  [[nodiscard]] virtual std::optional<TableRowStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class TableCellElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::table_cell;
  }

  [[nodiscard]] virtual Element *column(const Document *document,
                                        const DocumentCursor *cursor) = 0;
  [[nodiscard]] virtual Element *row(const Document *document,
                                     const DocumentCursor *cursor) = 0;

  [[nodiscard]] virtual bool covered(const Document *document) const = 0;
  [[nodiscard]] virtual TableDimensions
  span(const Document *document) const = 0;
  [[nodiscard]] virtual ValueType
  value_type(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<TableCellStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class FrameElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::frame;
  }

  [[nodiscard]] virtual AnchorType
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

  [[nodiscard]] virtual std::optional<GraphicStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class RectElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::rect;
  }

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<GraphicStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class LineElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::line;
  }

  [[nodiscard]] virtual std::string x1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y1(const Document *document) const = 0;
  [[nodiscard]] virtual std::string x2(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y2(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<GraphicStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class CircleElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::circle;
  }

  [[nodiscard]] virtual std::string x(const Document *document) const = 0;
  [[nodiscard]] virtual std::string y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<GraphicStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class CustomShapeElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::custom_shape;
  }

  [[nodiscard]] virtual std::optional<std::string>
  x(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  y(const Document *document) const = 0;
  [[nodiscard]] virtual std::string width(const Document *document) const = 0;
  [[nodiscard]] virtual std::string height(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<GraphicStyle>
  style(const Document *document, const DocumentCursor *cursor) const = 0;
};

class ImageElement : public virtual Element {
public:
  [[nodiscard]] ElementType type(const Document *) const override {
    return ElementType::image;
  }

  [[nodiscard]] virtual bool internal(const Document *document) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  file(const Document *document) const = 0;
  [[nodiscard]] virtual std::string href(const Document *document) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
