#include <common/document_style.h>
#include <odr/document_style.h>
#include <odr/property.h>

namespace odr {

Style::Style() = default;

Style::Style(std::shared_ptr<void> impl) : m_impl{std::move(impl)} {}

bool Style::operator==(const Style &rhs) const { return m_impl == rhs.m_impl; }

bool Style::operator!=(const Style &rhs) const { return m_impl != rhs.m_impl; }

Style::operator bool() const { return m_impl.operator bool(); }

PageStyle::PageStyle() = default;

PageStyle::PageStyle(std::shared_ptr<common::PageStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property PageStyle::width() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->width());
}

Property PageStyle::height() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->height());
}

Property PageStyle::marginTop() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginTop());
}

Property PageStyle::marginBottom() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginBottom());
}

Property PageStyle::marginLeft() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginLeft());
}

Property PageStyle::marginRight() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginRight());
}

Property PageStyle::printOrientation() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->printOrientation());
}

TextStyle::TextStyle() = default;

TextStyle::TextStyle(std::shared_ptr<common::TextStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TextStyle::fontName() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fontName());
}

Property TextStyle::fontSize() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fontSize());
}

Property TextStyle::fontWeight() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fontWeight());
}

Property TextStyle::fontStyle() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fontStyle());
}

Property TextStyle::fontColor() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fontColor());
}

Property TextStyle::backgroundColor() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->backgroundColor());
}

ParagraphStyle::ParagraphStyle() = default;

ParagraphStyle::ParagraphStyle(std::shared_ptr<common::ParagraphStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property ParagraphStyle::textAlign() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->textAlign());
}

Property ParagraphStyle::marginTop() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginTop());
}

Property ParagraphStyle::marginBottom() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginBottom());
}

Property ParagraphStyle::marginLeft() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginLeft());
}

Property ParagraphStyle::marginRight() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->marginRight());
}

TableStyle::TableStyle() = default;

TableStyle::TableStyle(std::shared_ptr<common::TableStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableStyle::width() const { return Property(m_impl->width()); }

TableColumnStyle::TableColumnStyle() = default;

TableColumnStyle::TableColumnStyle(
    std::shared_ptr<common::TableColumnStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableColumnStyle::width() const { return Property(m_impl->width()); }

TableCellStyle::TableCellStyle() = default;

TableCellStyle::TableCellStyle(std::shared_ptr<common::TableCellStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableCellStyle::paddingTop() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->paddingTop());
}

Property TableCellStyle::paddingBottom() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->paddingBottom());
}

Property TableCellStyle::paddingLeft() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->paddingLeft());
}

Property TableCellStyle::paddingRight() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->paddingRight());
}

Property TableCellStyle::borderTop() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->borderTop());
}

Property TableCellStyle::borderBottom() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->borderBottom());
}

Property TableCellStyle::borderLeft() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->borderLeft());
}

Property TableCellStyle::borderRight() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->borderRight());
}

DrawingStyle::DrawingStyle() = default;

DrawingStyle::DrawingStyle(std::shared_ptr<common::DrawingStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property DrawingStyle::strokeWidth() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->strokeWidth());
}

Property DrawingStyle::strokeColor() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->strokeColor());
}

Property DrawingStyle::fillColor() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->fillColor());
}

Property DrawingStyle::verticalAlign() const {
  if (!operator bool())
    return Property();
  return Property(m_impl->verticalAlign());
}

} // namespace odr
