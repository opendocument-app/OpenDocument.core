#ifndef ODR_COMMON_DOCUMENTELEMENTS_H
#define ODR_COMMON_DOCUMENTELEMENTS_H

#include <memory>
#include <optional>

namespace odr {
class ImageFile;
enum class ElementType;
enum class DocumentType;
struct PageProperties;
struct ParagraphProperties;
struct TextProperties;
struct TableProperties;
struct TableColumnProperties;
struct TableRowProperties;
struct TableCellProperties;
struct FrameProperties;
struct DrawingProperties;

namespace common {

class Table;
class TableColumn;
class TableRow;
class TableCell;

class Element {
public:
  virtual ~Element() = default;

  virtual std::shared_ptr<const Element> parent() const = 0;
  virtual std::shared_ptr<const Element> firstChild() const = 0;
  virtual std::shared_ptr<const Element> previousSibling() const = 0;
  virtual std::shared_ptr<const Element> nextSibling() const = 0;

  virtual ElementType type() const = 0;
};

class Slide : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string name() const = 0;
  virtual std::string notes() const = 0;
  virtual PageProperties pageProperties() const = 0;
};

class Sheet : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string name() const = 0;
  virtual std::uint32_t rowCount() const = 0;
  virtual std::uint32_t columnCount() const = 0;
  virtual std::shared_ptr<const Table> table() const = 0;
};

class Page : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string name() const = 0;
  virtual PageProperties pageProperties() const = 0;
};

class TextElement : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string text() const = 0;
};

class Paragraph : public virtual Element {
public:
  ElementType type() const final;

  virtual ParagraphProperties paragraphProperties() const = 0;
  virtual TextProperties textProperties() const = 0;
};

class Span : public virtual Element {
public:
  ElementType type() const final;

  virtual TextProperties textProperties() const = 0;
};

class Link : public virtual Element {
public:
  ElementType type() const final;

  virtual TextProperties textProperties() const = 0;

  virtual std::string href() const = 0;
};

class Bookmark : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string name() const = 0;
};

class List : public virtual Element {
public:
  ElementType type() const final;
};

class ListItem : public virtual Element {
public:
  ElementType type() const final;
};

class Table : public virtual Element {
public:
  ElementType type() const final;

  virtual std::uint32_t rowCount() const = 0;
  virtual std::uint32_t columnCount() const = 0;

  virtual std::shared_ptr<const TableColumn> firstColumn() const = 0;
  virtual std::shared_ptr<const TableRow> firstRow() const = 0;

  virtual TableProperties tableProperties() const = 0;
};

class TableColumn : public virtual Element {
public:
  ElementType type() const final;

  virtual TableColumnProperties tableColumnProperties() const = 0;
};

class TableRow : public virtual Element {
public:
  ElementType type() const final;

  virtual TableRowProperties tableRowProperties() const = 0;
};

class TableCell : public virtual Element {
public:
  ElementType type() const final;

  virtual std::uint32_t rowSpan() const = 0;
  virtual std::uint32_t columnSpan() const = 0;

  virtual TableCellProperties tableCellProperties() const = 0;
};

class Frame : public virtual Element {
public:
  ElementType type() const final;

  virtual FrameProperties frameProperties() const = 0;
};

class Image : public virtual Element {
public:
  ElementType type() const final;

  virtual bool internal() const = 0;
  virtual std::string href() const = 0;
  virtual odr::ImageFile imageFile() const = 0;
};

class Rect : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string x() const = 0;
  virtual std::string y() const = 0;
  virtual std::string width() const = 0;
  virtual std::string height() const = 0;

  virtual DrawingProperties drawingProperties() const = 0;
};

class Line : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string x1() const = 0;
  virtual std::string y1() const = 0;
  virtual std::string x2() const = 0;
  virtual std::string y2() const = 0;

  virtual DrawingProperties drawingProperties() const = 0;
};

class Circle : public virtual Element {
public:
  ElementType type() const final;

  virtual std::string x() const = 0;
  virtual std::string y() const = 0;
  virtual std::string width() const = 0;
  virtual std::string height() const = 0;

  virtual DrawingProperties drawingProperties() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENTELEMENTS_H
