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
  [[nodiscard]] virtual std::shared_ptr<Property> margin_top() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_bottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_left() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_right() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> print_orientation() const = 0;
};

class TextStyle {
public:
  virtual ~TextStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> font_name() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> font_size() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> font_weight() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> font_style() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> font_color() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Property> background_color() const = 0;
};

class ParagraphStyle {
public:
  virtual ~ParagraphStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> text_align() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_top() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_bottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_left() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> margin_right() const = 0;
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

  [[nodiscard]] virtual std::shared_ptr<Property> padding_top() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> padding_bottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> padding_left() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> padding_right() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> border_top() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> border_bottom() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> border_left() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> border_right() const = 0;
};

class DrawingStyle {
public:
  virtual ~DrawingStyle() = default;

  [[nodiscard]] virtual std::shared_ptr<Property> stroke_width() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> stroke_color() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> fill_color() const = 0;
  [[nodiscard]] virtual std::shared_ptr<Property> vertical_align() const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_DOCUMENT_STYLE_H
