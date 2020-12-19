#include <common/Property.h>
#include <odr/Property.h>

namespace odr {

Property::Property(std::shared_ptr<const common::Property> impl)
    : m_impl{std::move(impl)} {}

bool Property::operator==(const Property &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Property::operator!=(const Property &rhs) const {
  return m_impl != rhs.m_impl;
}

Property::operator bool() const { return value().operator bool(); }

std::optional<std::string> Property::value() const { return m_impl->value(); }

void Property::set(std::optional<std::string> value) const {
  m_impl->set(std::move(value));
}

} // namespace odr
