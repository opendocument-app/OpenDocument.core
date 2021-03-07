#include <internal/abstract/document_style.h>
#include <odr/experimental/document_style.h>
#include <odr/experimental/property.h>

namespace odr::experimental {

Style::Style() = default;

Style::Style(std::shared_ptr<void> impl) : m_impl{std::move(impl)} {}

bool Style::operator==(const Style &rhs) const { return m_impl == rhs.m_impl; }

bool Style::operator!=(const Style &rhs) const { return m_impl != rhs.m_impl; }

Style::operator bool() const { return m_impl.operator bool(); }

PageStyle::PageStyle() = default;

PageStyle::PageStyle(std::shared_ptr<internal::abstract::PageStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property PageStyle::width() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->width());
}

Property PageStyle::height() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->height());
}

Property PageStyle::margin_top() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_top());
}

Property PageStyle::margin_bottom() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_bottom());
}

Property PageStyle::margin_left() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_left());
}

Property PageStyle::margin_right() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_right());
}

Property PageStyle::print_orientation() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->print_orientation());
}

TextStyle::TextStyle() = default;

TextStyle::TextStyle(std::shared_ptr<internal::abstract::TextStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TextStyle::font_name() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->font_name());
}

Property TextStyle::font_size() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->font_size());
}

Property TextStyle::font_weight() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->font_weight());
}

Property TextStyle::font_style() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->font_style());
}

Property TextStyle::font_color() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->font_color());
}

Property TextStyle::background_color() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->background_color());
}

ParagraphStyle::ParagraphStyle() = default;

ParagraphStyle::ParagraphStyle(
    std::shared_ptr<internal::abstract::ParagraphStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property ParagraphStyle::text_align() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->text_align());
}

Property ParagraphStyle::margin_top() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_top());
}

Property ParagraphStyle::margin_bottom() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_bottom());
}

Property ParagraphStyle::margin_left() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_left());
}

Property ParagraphStyle::margin_right() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->margin_right());
}

TableStyle::TableStyle() = default;

TableStyle::TableStyle(std::shared_ptr<internal::abstract::TableStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableStyle::width() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->width());
}

TableColumnStyle::TableColumnStyle() = default;

TableColumnStyle::TableColumnStyle(
    std::shared_ptr<internal::abstract::TableColumnStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableColumnStyle::width() const { return Property(m_impl->width()); }

TableCellStyle::TableCellStyle() = default;

TableCellStyle::TableCellStyle(
    std::shared_ptr<internal::abstract::TableCellStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property TableCellStyle::padding_top() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->padding_top());
}

Property TableCellStyle::padding_bottom() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->padding_bottom());
}

Property TableCellStyle::padding_left() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->padding_left());
}

Property TableCellStyle::padding_right() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->padding_right());
}

Property TableCellStyle::border_top() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->border_top());
}

Property TableCellStyle::border_bottom() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->border_bottom());
}

Property TableCellStyle::border_left() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->border_left());
}

Property TableCellStyle::border_right() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->border_right());
}

DrawingStyle::DrawingStyle() = default;

DrawingStyle::DrawingStyle(
    std::shared_ptr<internal::abstract::DrawingStyle> impl)
    : Style(impl), m_impl{std::move(impl)} {}

Property DrawingStyle::stroke_width() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->stroke_width());
}

Property DrawingStyle::stroke_color() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->stroke_color());
}

Property DrawingStyle::fill_color() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->fill_color());
}

Property DrawingStyle::vertical_align() const {
  if (!m_impl) {
    return Property();
  }
  return Property(m_impl->vertical_align());
}

} // namespace odr::experimental
