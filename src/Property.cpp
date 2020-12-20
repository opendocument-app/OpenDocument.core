#include <common/Property.h>
#include <odr/Property.h>

namespace odr {

Property::Property() = default;

Property::Property(std::shared_ptr<common::Property> impl)
    : m_impl{std::move(impl)} {}

bool Property::operator==(const Property &rhs) const {
  return m_impl == rhs.m_impl;
}

bool Property::operator!=(const Property &rhs) const {
  return m_impl != rhs.m_impl;
}

Property::operator bool() const {
  return m_impl.operator bool() && value().operator bool();
}

Property::operator const char *() const { return value()->c_str(); }

std::string Property::operator*() const { return *value(); }

Property::operator std::string() const { return *value(); }

bool Property::readonly() const noexcept { return m_impl->readonly(); }

bool Property::optional() const noexcept { return m_impl->optional(); }

std::optional<std::string> Property::value() const { return m_impl->value(); }

void Property::set(std::optional<std::string> value) const {
  m_impl->set(std::move(value));
}

} // namespace odr
