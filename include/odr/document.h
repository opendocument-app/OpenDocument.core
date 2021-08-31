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
class Element;
class Style;
class Property;
} // namespace odr::internal::abstract

namespace odr {

enum class DocumentType;
class File;
class DocumentFile;

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

struct TableDimensions;
class DocumentCursor;
class Style;

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

class Element final {
public:
  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] std::optional<std::string> style_name() const;
  [[nodiscard]] Style style() const;

  class Extension {
  public:
    Extension(const internal::abstract::Document *document,
              const internal::abstract::Element *element,
              const void *extension);

  protected:
    const internal::abstract::Document *m_document;
    const internal::abstract::Element *m_element;
    const void *m_extension;
  };

  class Text final : public Extension {
  public:
    Text(const internal::abstract::Document *document,
         const internal::abstract::Element *element, const void *extension);

    [[nodiscard]] std::string value() const;
  };

  class Table final : public Extension {
  public:
    Table(const internal::abstract::Document *document,
          const internal::abstract::Element *element, const void *extension);

    [[nodiscard]] TableDimensions dimensions() const;
  };

  class TableCell final : public Extension {
  public:
    TableCell(const internal::abstract::Document *document,
              const internal::abstract::Element *element,
              const void *extension);

    [[nodiscard]] TableDimensions span() const;
  };

  class Image final : public Extension {
  public:
    Image(const internal::abstract::Document *document,
          const internal::abstract::Element *element, const void *extension);

    [[nodiscard]] bool internal() const;
    [[nodiscard]] std::optional<odr::File> file() const;
  };

  [[nodiscard]] Text text() const;
  [[nodiscard]] Table table() const;
  [[nodiscard]] TableCell table_cell() const;
  [[nodiscard]] Image image() const;

private:
  const internal::abstract::Document *m_document;
  const internal::abstract::Element *m_element;

  Element(const internal::abstract::Document *document,
          const internal::abstract::Element *element);
};

class DocumentCursor final {
public:
  bool operator==(const DocumentCursor &rhs) const;
  bool operator!=(const DocumentCursor &rhs) const;

  [[nodiscard]] std::string document_path() const;

  [[nodiscard]] ElementType element_type() const;

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

  DocumentCursor(std::shared_ptr<internal::abstract::Document> document,
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

class Property {
public:
  Property(const internal::abstract::Document *document,
           const internal::abstract::Element *element,
           const internal::abstract::Style *style,
           const internal::abstract::Property *property);

  [[nodiscard]] std::optional<std::string> value() const;

private:
  const internal::abstract::Document *m_document;
  const internal::abstract::Element *m_element;
  const internal::abstract::Style *m_style;
  const internal::abstract::Property *m_property;
};

class Style {
public:
  Style(const internal::abstract::Document *document,
        const internal::abstract::Element *element,
        internal::abstract::Style *style);

  class DirectionalProperty final {
  public:
    DirectionalProperty(const internal::abstract::Document *document,
                        const internal::abstract::Style *style,
                        const internal::abstract::Element *element,
                        void *m_property);

    [[nodiscard]] Property right() const;
    [[nodiscard]] Property top() const;
    [[nodiscard]] Property left() const;
    [[nodiscard]] Property bottom() const;

  protected:
    const internal::abstract::Document *m_document;
    const internal::abstract::Style *m_style;
    const internal::abstract::Element *m_element;
    void *m_property;
  };

  class Extension {
  public:
    Extension(const internal::abstract::Document *document,
              const internal::abstract::Element *element,
              const internal::abstract::Style *style, void *extension);

  protected:
    const internal::abstract::Document *m_document;
    const internal::abstract::Element *m_element;
    const internal::abstract::Style *m_style;
    void *m_extension;
  };

  class Text final : public Extension {
  public:
    Text(const internal::abstract::Document *document,
         const internal::abstract::Element *element,
         const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property font_name() const;
    [[nodiscard]] Property font_size() const;
    [[nodiscard]] Property font_weight() const;
    [[nodiscard]] Property font_style() const;
    [[nodiscard]] Property font_underline() const;
    [[nodiscard]] Property font_line_through() const;
    [[nodiscard]] Property font_shadow() const;
    [[nodiscard]] Property font_color() const;
    [[nodiscard]] Property background_color() const;
  };

  class Paragraph final : public Extension {
  public:
    Paragraph(const internal::abstract::Document *document,
              const internal::abstract::Element *element,
              const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property text_align() const;
    [[nodiscard]] DirectionalProperty margin() const;
  };

  class Table final : public Extension {
  public:
    Table(const internal::abstract::Document *document,
          const internal::abstract::Element *element,
          const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property width() const;
  };

  class TableColumn final : public Extension {
  public:
    TableColumn(const internal::abstract::Document *document,
                const internal::abstract::Element *element,
                const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property width() const;
  };

  class TableRow final : public Extension {
  public:
    TableRow(const internal::abstract::Document *document,
             const internal::abstract::Element *element,
             const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property height() const;
  };

  class TableCell final : public Extension {
  public:
    TableCell(const internal::abstract::Document *document,
              const internal::abstract::Element *element,
              const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property vertical_align() const;
    [[nodiscard]] Property background_color() const;
    [[nodiscard]] DirectionalProperty *padding() const;
    [[nodiscard]] DirectionalProperty *border() const;
  };

  class Graphic final : public Extension {
  public:
    Graphic(const internal::abstract::Document *document,
            const internal::abstract::Element *element,
            const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property stroke_width() const;
    [[nodiscard]] Property stroke_color() const;
    [[nodiscard]] Property fill_color() const;
    [[nodiscard]] Property vertical_align() const;
  };

  class PageLayout final : public Extension {
  public:
    PageLayout(const internal::abstract::Document *document,
               const internal::abstract::Element *element,
               const internal::abstract::Style *style, void *extension);

    [[nodiscard]] Property width() const;
    [[nodiscard]] Property height() const;
    [[nodiscard]] Property print_orientation() const;
    [[nodiscard]] DirectionalProperty margin() const;
  };

  [[nodiscard]] Text text() const;
  [[nodiscard]] Paragraph paragraph() const;
  [[nodiscard]] Table table() const;
  [[nodiscard]] TableColumn table_column() const;
  [[nodiscard]] TableRow table_row() const;
  [[nodiscard]] TableCell table_cell() const;
  [[nodiscard]] Graphic graphic() const;
  [[nodiscard]] PageLayout page_layout() const;

private:
  const internal::abstract::Document *m_document;
  const internal::abstract::Element *m_element;
  internal::abstract::Style *m_style;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
