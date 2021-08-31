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
enum class ElementProperty;
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
class Style;

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

class Property {
public:
  virtual ~Property() = default;

  [[nodiscard]] virtual std::optional<std::string>
  value(const Document *document, const Element *element,
        const Style *style) const = 0;
};

class Element {
public:
  using Allocator = std::function<void *(std::size_t size)>;

  virtual ~Element() = default;

  [[nodiscard]] virtual bool equals(const Document *document,
                                    const Element &other) const = 0;

  [[nodiscard]] virtual ElementType type(const Document *document) const = 0;

  [[nodiscard]] virtual std::optional<std::string>
  style_name(const Document *document) const = 0;
  [[nodiscard]] virtual Style *style(const Document *document) const = 0;

  virtual Element *parent(const Document *document,
                          const Allocator &allocator) = 0;
  virtual Element *first_child(const Document *document,
                               const Allocator &allocator) = 0;
  virtual Element *previous_sibling(const Document *document,
                                    const Allocator &allocator) = 0;
  virtual Element *next_sibling(const Document *document,
                                const Allocator &allocator) = 0;

  class Text {
  public:
    virtual ~Text() = default;
    [[nodiscard]] virtual std::string value(const Document *document,
                                            const Element *element) const = 0;
  };

  class Slide {
  public:
    virtual ~Slide() = default;
    [[nodiscard]] virtual Element *
    master_page(const Document *document, const Element *element,
                const Allocator &allocator) const = 0;
  };

  class Table {
  public:
    virtual ~Table() = default;
    [[nodiscard]] virtual TableDimensions
    dimensions(const Document *document, const Element *element) const = 0;
    [[nodiscard]] virtual Element *
    first_column(const Document *document, const Element *element,
                 const Allocator &allocator) const = 0;
    [[nodiscard]] virtual Element *
    first_row(const Document *document, const Element *element,
              const Allocator &allocator) const = 0;
  };

  class TableCell {
  public:
    virtual ~TableCell() = default;
    [[nodiscard]] virtual TableDimensions
    span(const Document *document, const Element *element) const = 0;
  };

  class Image {
  public:
    virtual ~Image() = default;
    [[nodiscard]] virtual bool internal(const Document *document,
                                        const Element *element) const = 0;
    [[nodiscard]] virtual std::optional<odr::File>
    file(const Document *document, const Element *element) const = 0;
  };

  [[nodiscard]] virtual const Text *text(const Document *document) const = 0;
  [[nodiscard]] virtual const Slide *slide(const Document *document) const = 0;
  [[nodiscard]] virtual const Table *table(const Document *document) const = 0;
  [[nodiscard]] virtual const TableCell *
  table_cell(const Document *document) const = 0;
  [[nodiscard]] virtual const Image *image(const Document *document) const = 0;
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

  class Slide {
  public:
    virtual ~Slide() = default;
    [[nodiscard]] virtual bool
    move_to_master_page(DocumentCursor *cursor,
                        const Element *element) const = 0;
  };

  class Table {
  public:
    virtual ~Table() = default;
    [[nodiscard]] virtual bool
    move_to_first_column(DocumentCursor *cursor,
                         const Element *element) const = 0;
    [[nodiscard]] virtual bool
    move_to_first_row(DocumentCursor *cursor, const Element *element) const = 0;
  };

  [[nodiscard]] virtual const Slide *slide() const = 0;
  [[nodiscard]] virtual const Table *table() const = 0;
};

class Style {
public:
  virtual ~Style() = default;

  class DirectionalProperty {
  public:
    virtual ~DirectionalProperty() = default;

    [[nodiscard]] virtual Property *right(const Document *document,
                                          const Style *style) = 0;
    [[nodiscard]] virtual Property *top(const Document *document,
                                        const Style *style) = 0;
    [[nodiscard]] virtual Property *left(const Document *document,
                                         const Style *style) = 0;
    [[nodiscard]] virtual Property *bottom(const Document *document,
                                           const Style *style) = 0;
  };

  class Text {
  public:
    virtual ~Text() = default;

    [[nodiscard]] virtual Property *font_name(const Document *document,
                                              const Style *style) = 0;
    [[nodiscard]] virtual Property *font_size(const Document *document,
                                              const Style *style) = 0;
    [[nodiscard]] virtual Property *font_weight(const Document *document,
                                                const Style *style) = 0;
    [[nodiscard]] virtual Property *font_style(const Document *document,
                                               const Style *style) = 0;
    [[nodiscard]] virtual Property *font_underline(const Document *document,
                                                   const Style *style) = 0;
    [[nodiscard]] virtual Property *font_line_through(const Document *document,
                                                      const Style *style) = 0;
    [[nodiscard]] virtual Property *font_shadow(const Document *document,
                                                const Style *style) = 0;
    [[nodiscard]] virtual Property *font_color(const Document *document,
                                               const Style *style) = 0;
    [[nodiscard]] virtual Property *background_color(const Document *document,
                                                     const Style *style) = 0;
  };

  class Paragraph {
  public:
    virtual ~Paragraph() = default;

    [[nodiscard]] virtual Property *text_align(const Document *document,
                                               const Style *style) = 0;
    [[nodiscard]] virtual DirectionalProperty *margin(const Document *document,
                                                      const Style *style) = 0;
  };

  class Table {
  public:
    virtual ~Table() = default;

    [[nodiscard]] virtual Property *width(const Document *document,
                                          const Style *style) = 0;
  };

  class TableColumn {
  public:
    virtual ~TableColumn() = default;

    [[nodiscard]] virtual Property *width(const Document *document,
                                          const Style *style) = 0;
  };

  class TableRow {
  public:
    virtual ~TableRow() = default;

    [[nodiscard]] virtual Property *height(const Document *document,
                                           const Style *style) = 0;
  };

  class TableCell {
  public:
    virtual ~TableCell() = default;

    [[nodiscard]] virtual Property *vertical_align(const Document *document,
                                                   const Style *style) = 0;
    [[nodiscard]] virtual Property *background_color(const Document *document,
                                                     const Style *style) = 0;
    [[nodiscard]] virtual DirectionalProperty *padding(const Document *document,
                                                       const Style *style) = 0;
    [[nodiscard]] virtual DirectionalProperty *border(const Document *document,
                                                      const Style *style) = 0;
  };

  class Graphic {
  public:
    virtual ~Graphic() = default;

    [[nodiscard]] virtual Property *stroke_width(const Document *document,
                                                 const Style *style) = 0;
    [[nodiscard]] virtual Property *stroke_color(const Document *document,
                                                 const Style *style) = 0;
    [[nodiscard]] virtual Property *fill_color(const Document *document,
                                               const Style *style) = 0;
    [[nodiscard]] virtual Property *vertical_align(const Document *document,
                                                   const Style *style) = 0;
  };

  class PageLayout {
  public:
    virtual ~PageLayout() = default;

    [[nodiscard]] virtual Property *width(const Document *document,
                                          const Style *style) = 0;
    [[nodiscard]] virtual Property *height(const Document *document,
                                           const Style *style) = 0;
    [[nodiscard]] virtual Property *print_orientation(const Document *document,
                                                      const Style *style) = 0;
    [[nodiscard]] virtual DirectionalProperty *margin(const Document *document,
                                                      const Style *style) = 0;
  };

  [[nodiscard]] virtual Text *text(const Document *document) = 0;
  [[nodiscard]] virtual Paragraph *paragraph(const Document *document) = 0;
  [[nodiscard]] virtual Table *table(const Document *document) = 0;
  [[nodiscard]] virtual TableColumn *table_column(const Document *document) = 0;
  [[nodiscard]] virtual TableRow *table_row(const Document *document) = 0;
  [[nodiscard]] virtual TableCell *table_cell(const Document *document) = 0;
  [[nodiscard]] virtual Graphic *graphic(const Document *document) = 0;
  [[nodiscard]] virtual PageLayout *page_layout(const Document *document) = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
