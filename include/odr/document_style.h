#ifndef ODR_DOCUMENT_STYLE_H
#define ODR_DOCUMENT_STYLE_H

#include <memory>

namespace odr::common {
class PageStyle;
class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;
} // namespace odr::common

namespace odr {
class Property;

class PageStyle final {
public:
  explicit PageStyle(std::shared_ptr<common::PageStyle> impl);

  [[nodiscard]] Property width() const;
  [[nodiscard]] Property height() const;
  [[nodiscard]] Property marginTop() const;
  [[nodiscard]] Property marginBottom() const;
  [[nodiscard]] Property marginLeft() const;
  [[nodiscard]] Property marginRight() const;
  [[nodiscard]] Property printOrientation() const;

private:
  std::shared_ptr<common::PageStyle> m_impl;
};

class TextStyle final {
public:
  explicit TextStyle(std::shared_ptr<common::TextStyle> impl);

  [[nodiscard]] Property fontName() const;
  [[nodiscard]] Property fontSize() const;
  [[nodiscard]] Property fontWeight() const;
  [[nodiscard]] Property fontStyle() const;
  [[nodiscard]] Property fontColor() const;

  [[nodiscard]] Property backgroundColor() const;

private:
  std::shared_ptr<common::TextStyle> m_impl;
};

class ParagraphStyle final {
public:
  explicit ParagraphStyle(std::shared_ptr<common::ParagraphStyle> impl);

  [[nodiscard]] Property textAlign() const;
  [[nodiscard]] Property marginTop() const;
  [[nodiscard]] Property marginBottom() const;
  [[nodiscard]] Property marginLeft() const;
  [[nodiscard]] Property marginRight() const;

private:
  std::shared_ptr<common::ParagraphStyle> m_impl;
};

class TableStyle final {
public:
  explicit TableStyle(std::shared_ptr<common::TableStyle> impl);

  [[nodiscard]] Property width() const;

private:
  std::shared_ptr<common::TableStyle> m_impl;
};

class TableColumnStyle final {
public:
  explicit TableColumnStyle(std::shared_ptr<common::TableColumnStyle> impl);

  [[nodiscard]] Property width() const;

private:
  std::shared_ptr<common::TableColumnStyle> m_impl;
};

class TableCellStyle final {
public:
  explicit TableCellStyle(std::shared_ptr<common::TableCellStyle> impl);

  [[nodiscard]] Property paddingTop() const;
  [[nodiscard]] Property paddingBottom() const;
  [[nodiscard]] Property paddingLeft() const;
  [[nodiscard]] Property paddingRight() const;
  [[nodiscard]] Property borderTop() const;
  [[nodiscard]] Property borderBottom() const;
  [[nodiscard]] Property borderLeft() const;
  [[nodiscard]] Property borderRight() const;

private:
  std::shared_ptr<common::TableCellStyle> m_impl;
};

class DrawingStyle final {
public:
  explicit DrawingStyle(std::shared_ptr<common::DrawingStyle> impl);

  [[nodiscard]] Property strokeWidth() const;
  [[nodiscard]] Property strokeColor() const;
  [[nodiscard]] Property fillColor() const;
  [[nodiscard]] Property verticalAlign() const;

private:
  std::shared_ptr<common::DrawingStyle> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENT_STYLE_H
