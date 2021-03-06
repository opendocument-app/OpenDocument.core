#include <internal/common/property.h>
#include <odr/exceptions.h>

namespace odr::internal::common {

ConstProperty::ConstProperty() = default;

ConstProperty::ConstProperty(std::optional<std::string> value)
    : m_value{value} {}

bool ConstProperty::readonly() const noexcept { return true; }

bool ConstProperty::optional() const noexcept { return true; }

std::optional<std::string> ConstProperty::value() const { return m_value; }

void ConstProperty::set(std::optional<std::string>) {
  throw PropertyReadOnly();
}

XmlAttributeProperty::XmlAttributeProperty(pugi::xml_attribute attribute)
    : m_attribute{attribute} {}

bool XmlAttributeProperty::readonly() const noexcept { return false; }

bool XmlAttributeProperty::optional() const noexcept { return false; }

std::optional<std::string> XmlAttributeProperty::value() const {
  return m_attribute.value();
}

void XmlAttributeProperty::set(std::optional<std::string> value) {
  if (!value)
    throw PropertyNotOptional();
  m_attribute = value->c_str();
}

XmlOptionalAttributeProperty::XmlOptionalAttributeProperty(
    pugi::xml_node node, std::string attributeName)
    : m_node{node}, m_attributeName{std::move(attributeName)} {}

bool XmlOptionalAttributeProperty::readonly() const noexcept { return false; }

bool XmlOptionalAttributeProperty::optional() const noexcept { return true; }

std::optional<std::string> XmlOptionalAttributeProperty::value() const {
  if (auto attr = attribute(); attr)
    return attr.value();
  return {};
}

void XmlOptionalAttributeProperty::set(std::optional<std::string> value) {
  auto attr = attribute();
  if (!value && attr)
    m_node.remove_attribute(attr);
  else if (value && !attr)
    attr = m_node.append_attribute(m_attributeName.c_str());
  if (value && attr)
    attr.set_value(value->c_str());
}

pugi::xml_attribute XmlOptionalAttributeProperty::attribute() const {
  return m_node.attribute(m_attributeName.c_str());
}

} // namespace odr::internal::common
