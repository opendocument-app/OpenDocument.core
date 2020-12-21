#include <common/document_style.h>
#include <odr/document_style.h>
#include <odr/property.h>

namespace odr {

PageStyle::PageStyle(std::shared_ptr<common::PageStyle> impl)
    : m_impl{std::move(impl)} {}

Property PageStyle::width() const { return Property(m_impl->width()); }

Property PageStyle::height() const { return Property(m_impl->height()); }

Property PageStyle::marginTop() const { return Property(m_impl->marginTop()); }

Property PageStyle::marginBottom() const {
  return Property(m_impl->marginBottom());
}

Property PageStyle::marginLeft() const {
  return Property(m_impl->marginLeft());
}

Property PageStyle::marginRight() const {
  return Property(m_impl->marginRight());
}

Property PageStyle::printOrientation() const {
  return Property(m_impl->printOrientation());
}

TextStyle::TextStyle(std::shared_ptr<common::TextStyle> impl)
    : m_impl{std::move(impl)} {}

Property TextStyle::fontName() const { return Property(m_impl->fontName()); }

Property TextStyle::fontSize() const { return Property(m_impl->fontSize()); }

Property TextStyle::fontWeight() const {
  return Property(m_impl->fontWeight());
}

Property TextStyle::fontStyle() const { return Property(m_impl->fontStyle()); }

Property TextStyle::fontColor() const { return Property(m_impl->fontColor()); }

Property TextStyle::backgroundColor() const {
  return Property(m_impl->backgroundColor());
}

ParagraphStyle::ParagraphStyle(std::shared_ptr<common::ParagraphStyle> impl)
    : m_impl{std::move(impl)} {}

Property ParagraphStyle::textAlign() const {
  return Property(m_impl->textAlign());
}

Property ParagraphStyle::marginTop() const {
  return Property(m_impl->marginTop());
}

Property ParagraphStyle::marginBottom() const {
  return Property(m_impl->marginBottom());
}

Property ParagraphStyle::marginLeft() const {
  return Property(m_impl->marginLeft());
}

Property ParagraphStyle::marginRight() const {
  return Property(m_impl->marginRight());
}

TableStyle::TableStyle(std::shared_ptr<common::TableStyle> impl)
    : m_impl{std::move(impl)} {}

Property TableStyle::width() const { return Property(m_impl->width()); }

TableColumnStyle::TableColumnStyle(
    std::shared_ptr<common::TableColumnStyle> impl)
    : m_impl{std::move(impl)} {}

Property TableColumnStyle::width() const { return Property(m_impl->width()); }

TableCellStyle::TableCellStyle(std::shared_ptr<common::TableCellStyle> impl)
    : m_impl{std::move(impl)} {}

Property TableCellStyle::paddingTop() const {
  return Property(m_impl->paddingTop());
}

Property TableCellStyle::paddingBottom() const {
  return Property(m_impl->paddingBottom());
}

Property TableCellStyle::paddingLeft() const {
  return Property(m_impl->paddingLeft());
}

Property TableCellStyle::paddingRight() const {
  return Property(m_impl->paddingRight());
}

Property TableCellStyle::borderTop() const {
  return Property(m_impl->borderTop());
}

Property TableCellStyle::borderBottom() const {
  return Property(m_impl->borderBottom());
}

Property TableCellStyle::borderLeft() const {
  return Property(m_impl->borderLeft());
}

Property TableCellStyle::borderRight() const {
  return Property(m_impl->borderRight());
}

DrawingStyle::DrawingStyle(std::shared_ptr<common::DrawingStyle> impl)
    : m_impl{std::move(impl)} {}

Property DrawingStyle::strokeWidth() const {
  return Property(m_impl->strokeWidth());
}

Property DrawingStyle::strokeColor() const {
  return Property(m_impl->strokeColor());
}

Property DrawingStyle::fillColor() const {
  return Property(m_impl->fillColor());
}

Property DrawingStyle::verticalAlign() const {
  return Property(m_impl->verticalAlign());
}

} // namespace odr
