#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <any>
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

class DocumentCursor {
public:
  virtual ~DocumentCursor() = default;

  virtual bool operator==(const DocumentCursor &rhs) const = 0;
  virtual bool operator!=(const DocumentCursor &rhs) const = 0;

  [[nodiscard]] virtual std::unique_ptr<DocumentCursor> copy() const = 0;

  [[nodiscard]] virtual std::string document_path() const = 0;

  class Element {
  public:
    virtual ~Element() = default;

    virtual bool equals(const DocumentCursor *cursor,
                        const Element &other) const = 0;

    virtual bool move_to_parent(DocumentCursor *cursor) = 0;
    virtual bool move_to_first_child(DocumentCursor *cursor) = 0;
    virtual bool move_to_previous_sibling(DocumentCursor *cursor) = 0;
    virtual bool move_to_next_sibling(DocumentCursor *cursor) = 0;

    [[nodiscard]] virtual ElementType
    type(const DocumentCursor *cursor) const = 0;
    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(const DocumentCursor *cursor) const = 0;

    class Text {
    public:
      virtual ~Text() = default;
      [[nodiscard]] virtual std::string value(const DocumentCursor *cursor,
                                              const Element *element) const = 0;
    };

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
      [[nodiscard]] virtual TableDimensions
      table_dimensions(const DocumentCursor *cursor,
                       const Element *element) const = 0;
      [[nodiscard]] virtual bool
      move_to_first_table_column(DocumentCursor *cursor,
                                 const Element *element) const = 0;
      [[nodiscard]] virtual bool
      move_to_first_table_row(DocumentCursor *cursor,
                              const Element *element) const = 0;
    };

    class TableCell {
    public:
      virtual ~TableCell() = default;
      [[nodiscard]] virtual TableDimensions
      table_cell_span(const DocumentCursor *cursor,
                      const Element *element) const = 0;
    };

    class Image {
    public:
      virtual ~Image() = default;
      [[nodiscard]] virtual bool
      image_internal(const DocumentCursor *cursor,
                     const Element *element) const = 0;
      [[nodiscard]] virtual std::optional<odr::File>
      image_file(const DocumentCursor *cursor,
                 const Element *element) const = 0;
    };

    [[nodiscard]] virtual const Text *
    text(const DocumentCursor *cursor) const = 0;
    [[nodiscard]] virtual const Slide *
    slide(const DocumentCursor *cursor) const = 0;
    [[nodiscard]] virtual const Table *
    table(const DocumentCursor *cursor) const = 0;
    [[nodiscard]] virtual const TableCell *
    table_cell(const DocumentCursor *cursor) const = 0;
    [[nodiscard]] virtual const Image *
    image(const DocumentCursor *cursor) const = 0;
  };

  [[nodiscard]] virtual Element *element() = 0;
  [[nodiscard]] virtual const Element *element() const = 0;
};

class Style {
public:
  class Property {
  public:
    virtual ~Property() = default;

    [[nodiscard]] virtual std::string value(const DocumentCursor *cursor,
                                            const Style *style) const = 0;
  };

  class Text {
  public:
    virtual ~Text() = default;

    [[nodiscard]] virtual Property *font_name(const DocumentCursor *cursor,
                                              const Style *style) const = 0;
    [[nodiscard]] virtual Property *font_size(const DocumentCursor *cursor,
                                              const Style *style) const = 0;
    [[nodiscard]] virtual Property *font_weight(const DocumentCursor *cursor,
                                                const Style *style) const = 0;
    [[nodiscard]] virtual Property *font_style(const DocumentCursor *cursor,
                                               const Style *style) const = 0;
    [[nodiscard]] virtual Property *
    font_underline(const DocumentCursor *cursor, const Style *style) const = 0;
    [[nodiscard]] virtual Property *font_shadow(const DocumentCursor *cursor,
                                                const Style *style) const = 0;
    [[nodiscard]] virtual Property *font_color(const DocumentCursor *cursor,
                                               const Style *style) const = 0;
    [[nodiscard]] virtual Property *
    background_color(const DocumentCursor *cursor,
                     const Style *style) const = 0;
  };

  class Paragraph {
  public:
    virtual ~Paragraph() = default;

    [[nodiscard]] virtual Property *text_align(const DocumentCursor *cursor,
                                               const Style *style) const = 0;
    [[nodiscard]] virtual Property *margin(const DocumentCursor *cursor,
                                           const Style *style) const = 0;
  };

  class Table {
  public:
    virtual ~Table() = default;

    [[nodiscard]] virtual Property *width(const DocumentCursor *cursor,
                                          const Style *style) const = 0;
  };

  class TableColumn {
  public:
    virtual ~TableColumn() = default;

    [[nodiscard]] virtual Property *width(const DocumentCursor *cursor,
                                          const Style *style) const = 0;
  };

  class TableRow {
  public:
    virtual ~TableRow() = default;

    [[nodiscard]] virtual Property *height(const DocumentCursor *cursor,
                                           const Style *style) const = 0;
  };

  class TableCell {
  public:
    virtual ~TableCell() = default;

    [[nodiscard]] virtual Property *
    vertical_align(const DocumentCursor *cursor, const Style *style) const = 0;
    [[nodiscard]] virtual Property *
    background_color(const DocumentCursor *cursor,
                     const Style *style) const = 0;
    [[nodiscard]] virtual Property *padding(const DocumentCursor *cursor,
                                            const Style *style) const = 0;
    [[nodiscard]] virtual Property *border(const DocumentCursor *cursor,
                                           const Style *style) const = 0;
  };

  class Graphic {
  public:
    virtual ~Graphic() = default;

    [[nodiscard]] virtual Property *stroke_width(const DocumentCursor *cursor,
                                                 const Style *style) const = 0;
    [[nodiscard]] virtual Property *stroke_color(const DocumentCursor *cursor,
                                                 const Style *style) const = 0;
    [[nodiscard]] virtual Property *fill_color(const DocumentCursor *cursor,
                                               const Style *style) const = 0;
    [[nodiscard]] virtual Property *
    vertical_align(const DocumentCursor *cursor, const Style *style) const = 0;
  };

  class PageLayout {
  public:
    virtual ~PageLayout() = default;

    [[nodiscard]] virtual Property *width(const DocumentCursor *cursor,
                                          const Style *style) const = 0;
    [[nodiscard]] virtual Property *height(const DocumentCursor *cursor,
                                           const Style *style) const = 0;
    [[nodiscard]] virtual Property *
    print_orientation(const DocumentCursor *cursor,
                      const Style *style) const = 0;
    [[nodiscard]] virtual Property *margin(const DocumentCursor *cursor,
                                           const Style *style) const = 0;
  };

  virtual ~Style() = default;

  [[nodiscard]] virtual Text *text(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual Paragraph *
  paragraph(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual Table *table(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual TableColumn *
  table_column(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual TableRow *
  table_row(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual TableCell *
  table_cell(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual Graphic *
  graphic(const DocumentCursor *cursor) const = 0;
  [[nodiscard]] virtual PageLayout *
  page_layout(const DocumentCursor *cursor) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
