#ifndef ODR_COMMON_DOCUMENTELEMENTS_H
#define ODR_COMMON_DOCUMENTELEMENTS_H

#include <memory>
#include <optional>

namespace odr {
enum class ElementType;
enum class DocumentType;
struct PageProperties;
struct ParagraphProperties;
struct TextProperties;

namespace common {

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

class Image : public virtual Element {
public:
  ElementType type() const final;
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
};

class TableColumn : public virtual Element {
public:
  ElementType type() const final;
};

class TableRow : public virtual Element {
public:
  ElementType type() const final;
};

class TableCell : public virtual Element {
public:
  ElementType type() const final;

  virtual std::uint32_t rowSpan() const = 0;
  virtual std::uint32_t columnSpan() const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENTELEMENTS_H
