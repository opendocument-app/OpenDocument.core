#ifndef ODR_INTERNAL_ABSTRACT_ELEMENT_H
#define ODR_INTERNAL_ABSTRACT_ELEMENT_H

#include <any>
#include <string>
#include <unordered_map>

namespace odr {
class File;

enum class ElementType;
enum class ElementProperty;

class Element;

struct TableDimensions;
} // namespace odr

namespace odr::internal::common {
class TableRange;
} // namespace odr::internal::common

namespace odr::internal::abstract {

class Element {
public:
  virtual ~Element() = default;

  virtual bool operator==(const Element &rhs) const = 0;
  virtual bool operator!=(const Element &rhs) const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;

  [[nodiscard]] virtual odr::Element copy() const = 0;

  [[nodiscard]] virtual odr::Element parent() const = 0;
  [[nodiscard]] virtual odr::Element first_child() const = 0;
  [[nodiscard]] virtual odr::Element previous_sibling() const = 0;
  [[nodiscard]] virtual odr::Element next_sibling() const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties() const = 0;
};

class Slide : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::string notes() const = 0;
};

class Sheet : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual TableDimensions dimensions() const = 0;

  [[nodiscard]] virtual common::TableRange content_bounds() const = 0;
  [[nodiscard]] virtual common::TableRange
  content_bounds(common::TableRange within) const = 0;

  [[nodiscard]] virtual odr::Element column(std::uint32_t column) const = 0;
  [[nodiscard]] virtual odr::Element row(std::uint32_t row) const = 0;
  [[nodiscard]] virtual odr::Element cell(std::uint32_t row,
                                          std::uint32_t column) const = 0;
};

class Page : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
};

class TextElement : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string text() const = 0;
};

class LinkElement : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string href() const = 0;
};

class BookmarkElement : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string name() const = 0;
};

class TableElement : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual TableDimensions dimensions() const = 0;

  [[nodiscard]] virtual odr::Element first_column() const = 0;
  [[nodiscard]] virtual odr::Element first_row() const = 0;
};

class FrameElement : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string anchor_type() const = 0;
  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
  [[nodiscard]] virtual std::string z_index() const = 0;
};

class ImageElement : public Element {
public:
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual odr::File file() const = 0;
};

class RectElement : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
};

class LineElement : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x1() const = 0;
  [[nodiscard]] virtual std::string y1() const = 0;
  [[nodiscard]] virtual std::string x2() const = 0;
  [[nodiscard]] virtual std::string y2() const = 0;
};

class CircleElement : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
};

class CustomShapeElement : public Element {
  [[nodiscard]] ElementType type() const final;

  [[nodiscard]] virtual std::string x() const = 0;
  [[nodiscard]] virtual std::string y() const = 0;
  [[nodiscard]] virtual std::string width() const = 0;
  [[nodiscard]] virtual std::string height() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_ELEMENT_H
