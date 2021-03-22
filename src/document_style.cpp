#include <internal/abstract/document.h>
#include <odr/document_element_property.h>
#include <odr/document_element_property_value.h>
#include <odr/document_style.h>

namespace odr {

PageStyle::PageStyle() = default;

PageStyle::PageStyle(std::shared_ptr<const internal::abstract::Document> impl,
                     const std::uint64_t id)
    : m_impl{std::move(impl)}, m_id{id} {}

bool PageStyle::operator==(const PageStyle &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id;
}

bool PageStyle::operator!=(const PageStyle &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id;
}

PageStyle::operator bool() const { return m_impl.operator bool() && m_id != 0; }

ElementPropertyValue PageStyle::width() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::WIDTH);
}

ElementPropertyValue PageStyle::height() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::HEIGHT);
}

ElementPropertyValue PageStyle::margin_top() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_TOP);
}

ElementPropertyValue PageStyle::margin_bottom() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_BOTTOM);
}

ElementPropertyValue PageStyle::margin_left() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_LEFT);
}

ElementPropertyValue PageStyle::margin_right() const {
  return ElementPropertyValue(m_impl, m_id, ElementProperty::MARGIN_RIGHT);
}

} // namespace odr
