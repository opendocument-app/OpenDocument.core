#ifndef ODR_DOCUMENT_STYLE_H
#define ODR_DOCUMENT_STYLE_H

#include <memory>

namespace odr::internal::abstract {
class PageStyle;
class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;
} // namespace odr::internal::abstract

namespace odr {
class Property;

class Style {
public:
  Style();
  explicit Style(std::shared_ptr<void> impl);

  bool operator==(const Style &rhs) const;
  bool operator!=(const Style &rhs) const;
  explicit operator bool() const;

private:
  std::shared_ptr<void> m_impl;
};

class PageStyle final : public Style {
public:
  PageStyle();
  explicit PageStyle(std::shared_ptr<internal::abstract::PageStyle> impl);

  [[nodiscard]] Property width() const;
  [[nodiscard]] Property height() const;
  [[nodiscard]] Property margin_top() const;
  [[nodiscard]] Property margin_bottom() const;
  [[nodiscard]] Property margin_left() const;
  [[nodiscard]] Property margin_right() const;
  [[nodiscard]] Property print_orientation() const;

private:
  std::shared_ptr<internal::abstract::PageStyle> m_impl;
};

class TextStyle final : public Style {
public:
  TextStyle();
  explicit TextStyle(std::shared_ptr<internal::abstract::TextStyle> impl);

  [[nodiscard]] Property font_name() const;
  [[nodiscard]] Property font_size() const;
  [[nodiscard]] Property font_weight() const;
  [[nodiscard]] Property font_style() const;
  [[nodiscard]] Property font_color() const;

  [[nodiscard]] Property background_color() const;

private:
  std::shared_ptr<internal::abstract::TextStyle> m_impl;
};

class ParagraphStyle final : public Style {
public:
  ParagraphStyle();
  explicit ParagraphStyle(
      std::shared_ptr<internal::abstract::ParagraphStyle> impl);

  [[nodiscard]] Property text_align() const;
  [[nodiscard]] Property margin_top() const;
  [[nodiscard]] Property margin_bottom() const;
  [[nodiscard]] Property margin_left() const;
  [[nodiscard]] Property margin_right() const;

private:
  std::shared_ptr<internal::abstract::ParagraphStyle> m_impl;
};

class TableStyle final : public Style {
public:
  TableStyle();
  explicit TableStyle(std::shared_ptr<internal::abstract::TableStyle> impl);

  [[nodiscard]] Property width() const;

private:
  std::shared_ptr<internal::abstract::TableStyle> m_impl;
};

class TableColumnStyle final : public Style {
public:
  TableColumnStyle();
  explicit TableColumnStyle(
      std::shared_ptr<internal::abstract::TableColumnStyle> impl);

  [[nodiscard]] Property width() const;

private:
  std::shared_ptr<internal::abstract::TableColumnStyle> m_impl;
};

class TableCellStyle final : public Style {
public:
  TableCellStyle();
  explicit TableCellStyle(
      std::shared_ptr<internal::abstract::TableCellStyle> impl);

  [[nodiscard]] Property padding_top() const;
  [[nodiscard]] Property padding_bottom() const;
  [[nodiscard]] Property padding_left() const;
  [[nodiscard]] Property padding_right() const;
  [[nodiscard]] Property border_top() const;
  [[nodiscard]] Property border_bottom() const;
  [[nodiscard]] Property border_left() const;
  [[nodiscard]] Property border_right() const;

private:
  std::shared_ptr<internal::abstract::TableCellStyle> m_impl;
};

class DrawingStyle final : public Style {
public:
  DrawingStyle();
  explicit DrawingStyle(std::shared_ptr<internal::abstract::DrawingStyle> impl);

  [[nodiscard]] Property stroke_width() const;
  [[nodiscard]] Property stroke_color() const;
  [[nodiscard]] Property fill_color() const;
  [[nodiscard]] Property vertical_align() const;

private:
  std::shared_ptr<internal::abstract::DrawingStyle> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENT_STYLE_H
