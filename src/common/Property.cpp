#include <common/Property.h>
#include <odr/Exceptions.h>

namespace odr::common {

ConstProperty::ConstProperty() = default;

ConstProperty::ConstProperty(std::optional<std::string> value)
    : m_value{value} {}

bool ConstProperty::readonly() const noexcept { return true; }

bool ConstProperty::optional() const noexcept { return true; }

std::optional<std::string> ConstProperty::value() const { return m_value; }

void ConstProperty::set(std::optional<std::string>) {
  throw PropertyReadOnly();
}

} // namespace odr::common
