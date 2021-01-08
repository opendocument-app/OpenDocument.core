#ifndef ODR_ABSTRACT_DOCUMENT_STYLE_H
#define ODR_ABSTRACT_DOCUMENT_STYLE_H

#include <memory>

namespace odr::abstract {
class Property;

class PageStyle {
public:
  virtual ~PageStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> height() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginRight() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> printOrientation() const = 0;
};

class TextStyle {
public:
  virtual ~TextStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> fontName() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fontSize() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fontWeight() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fontStyle() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fontColor() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Property> backgroundColor() const = 0;
};

class ParagraphStyle {
public:
  virtual ~ParagraphStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> textAlign() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> marginRight() const = 0;
};

class TableStyle {
public:
  virtual ~TableStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
};

class TableColumnStyle {
public:
  virtual ~TableColumnStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> width() const = 0;
};

class TableCellStyle {
public:
  virtual ~TableCellStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> paddingTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> paddingRight() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderTop() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderBottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderLeft() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> borderRight() const = 0;
};

class DrawingStyle {
public:
  virtual ~DrawingStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> strokeWidth() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> strokeColor() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fillColor() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> verticalAlign() const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_DOCUMENT_STYLE_H
