#include <internal/util/property_util.h>

namespace odr::internal::util {

void property::set_optional_property(
    const ElementProperty property, std::any value,
    std::unordered_map<ElementProperty, std::any> &result) {
  if (value.has_value()) {
    result[property] = std::move(value);
  }
}

} // namespace odr::internal::util
