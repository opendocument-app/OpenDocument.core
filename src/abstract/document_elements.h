#ifndef ODR_ABSTRACT_DOCUMENT_ELEMENTS_H
#define ODR_ABSTRACT_DOCUMENT_ELEMENTS_H

#include <memory>
#include <optional>

namespace odr {
class ImageFile;
enum class ElementType;
} // namespace odr

namespace odr::abstract {
class Property;
class PageStyle;
class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;

class Table;
class TableColumn;
class TableRow;
class TableCell;

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual std::shared_ptr<const Element> parent() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element> first_child() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element>
  previous_sibling() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element> next_sibling() const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;
};

class Slide : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::string notes() const = 0;

  [[nodiscard]] virtual std::shared_ptr<PageStyle> page_style() const = 0;
};

class Sheet : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::uint32_t row_count() const = 0;
  [[nodiscard]] virtual std::uint32_t column_count() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Table> table() const = 0;
};

class Page : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;

  [[nodiscard]] virtual std::shared_ptr<PageStyle> page_style() const = 0;
};

class TextElement : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string text() const = 0;
};

class Paragraph : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<ParagraphStyle>
  paragraph_style() const = 0;
  [[nodiscard]] virtual std::shared_ptr<TextStyle> text_style() const = 0;
};

class Span : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<TextStyle> text_style() const = 0;
};

class Link : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<TextStyle> text_style() const = 0;

  [[nodiscard]] virtual std::string href() const = 0;
};

class Bookmark : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
};

class List : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;
};

class ListItem : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;
};

class Table : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::uint32_t row_count() const = 0;
  [[nodiscard]] virtual std::uint32_t column_count() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const TableColumn>
  first_column() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const TableRow> first_row() const = 0;

  [[nodiscard]] virtual std::shared_ptr<TableStyle> table_style() const = 0;
};

class TableColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<TableColumnStyle>
  table_column_style() const = 0;
};

class TableRow : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;
};

class TableCell : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::uint32_t row_span() const = 0;
  [[nodiscard]] virtual std::uint32_t column_span() const = 0;

  [[nodiscard]] virtual std::shared_ptr<TableCellStyle>
  table_cell_style() const = 0;
};

class Frame : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<Property> anchor_type() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> height() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> z_index() const = 0;
};

class Image : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual bool internal() const = 0;
  [[nodiscard]] virtual std::string href() const = 0;
  [[nodiscard]] virtual odr::ImageFile image_file() const = 0;
};

class Rect : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;

  [[nodiscard]] virtual std::shared_ptr<DrawingStyle> drawing_style() const = 0;
};

class Line : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x1() const = 0;
  [[nodiscard]] virtual std::string y1() const = 0;
  [[nodiscard]] virtual std::string x2() const = 0;
  [[nodiscard]] virtual std::string y2() const = 0;

  [[nodiscard]] virtual std::shared_ptr<DrawingStyle> drawing_style() const = 0;
};

class Circle : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;

  [[nodiscard]] virtual std::shared_ptr<DrawingStyle> drawing_style() const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_DOCUMENT_ELEMENTS_H
