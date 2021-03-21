#include <internal/abstract/document.h>
#include <odr/experimental/document_element_property_value.h>

namespace odr::experimental {

ElementPropertyValue::ElementPropertyValue() = default;

ElementPropertyValue::ElementPropertyValue(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id, const ElementProperty property)
    : m_impl{std::move(impl)}, m_id{id}, m_property{property} {}

bool ElementPropertyValue::operator==(const ElementPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id &&
         m_property == rhs.m_property;
}

bool ElementPropertyValue::operator!=(const ElementPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id ||
         m_property != rhs.m_property;
}

ElementPropertyValue::operator bool() const {
  return m_impl.operator bool() && m_id != 0;
}

std::any ElementPropertyValue::get() const {
  return m_impl->element_property(m_id, m_property);
}

std::string ElementPropertyValue::get_string() const {
  return m_impl->element_string_property(m_id, m_property);
}

std::uint32_t ElementPropertyValue::get_uint32() const {
  return m_impl->element_uint32_property(m_id, m_property);
}

bool ElementPropertyValue::get_bool() const {
  return m_impl->element_bool_property(m_id, m_property);
}

const char *ElementPropertyValue::get_optional_string() const {
  return m_impl->element_optional_string_property(m_id, m_property);
}

void ElementPropertyValue::set(const std::any &value) const {
  m_impl->set_element_property(m_id, m_property, value);
}

void ElementPropertyValue::set_string(const std::string &value) const {
  m_impl->set_element_property(m_id, m_property, value.c_str());
}

void ElementPropertyValue::remove() const {
  m_impl->remove_element_property(m_id, m_property);
}

} // namespace odr::experimental
