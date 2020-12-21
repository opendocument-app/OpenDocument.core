#ifndef ODR_COMMON_DOCUMENT_ELEMENTS_H
#define ODR_COMMON_DOCUMENT_ELEMENTS_H

#include <memory>
#include <optional>

namespace odr {
class ImageFile;
enum class ElementType;
struct PageProperties;
struct TextProperties;

namespace common {
class Property;
class Table;
class TableColumn;
class TableRow;
class TableCell;

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual std::shared_ptr<const Element> parent() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element> firstChild() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element>
  previousSibling() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Element> nextSibling() const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;
};

class Slide : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::string notes() const = 0;
  [[nodiscard]] virtual PageProperties pageProperties() const = 0;
};

class Sheet : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::uint32_t rowCount() const = 0;
  [[nodiscard]] virtual std::uint32_t columnCount() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const Table> table() const = 0;
};

class Page : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual PageProperties pageProperties() const = 0;
};

class TextElement : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string text() const = 0;
};

class Paragraph : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<Property> textAlign() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginRight() const = 0;
  [[nodiscard]] virtual TextProperties textProperties() const = 0;
};

class Span : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual TextProperties textProperties() const = 0;
};

class Link : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual TextProperties textProperties() const = 0;

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

  [[nodiscard]] virtual std::uint32_t rowCount() const = 0;
  [[nodiscard]] virtual std::uint32_t columnCount() const = 0;

  [[nodiscard]] virtual std::shared_ptr<const TableColumn>
  firstColumn() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const TableRow> firstRow() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
};

class TableColumn : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
};

class TableRow : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;
};

class TableCell : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::uint32_t rowSpan() const = 0;
  [[nodiscard]] virtual std::uint32_t columnSpan() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Property> paddingTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingRight() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderRight() const = 0;
};

class Frame : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::shared_ptr<Property> anchorType() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> height() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> zIndex() const = 0;
};

class Image : public virtual Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual bool internal() const = 0;
  [[nodiscard]] virtual std::string href() const = 0;
  [[nodiscard]] virtual odr::ImageFile imageFile() const = 0;
};

class DrawingElement : public virtual Element {
public:
  [[nodiscard]] virtual std::shared_ptr<Property> strokeWidth() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> strokeColor() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fillColor() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> verticalAlign() const = 0;
};

class Rect : public virtual DrawingElement {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
};

class Line : public virtual DrawingElement {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x1() const = 0;
  [[nodiscard]] virtual std::string y1() const = 0;
  [[nodiscard]] virtual std::string x2() const = 0;
  [[nodiscard]] virtual std::string y2() const = 0;
};

class Circle : public virtual DrawingElement {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_ELEMENTS_H
