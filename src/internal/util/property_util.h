#ifndef ODR_INTERNAL_PROPERTY_UTIL_H
#define ODR_INTERNAL_PROPERTY_UTIL_H

#include <any>
#include <unordered_map>

namespace odr {
enum class ElementProperty;
}

namespace odr::internal::util::property {

void set_optional_property(
    ElementProperty property, std::any value,
    std::unordered_map<ElementProperty, std::any> &result);

} // namespace odr::internal::util::property

#endif // ODR_INTERNAL_PROPERTY_UTIL_H
