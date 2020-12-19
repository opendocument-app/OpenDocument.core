#ifndef ODR_COMMON_PROPERTY_H
#define ODR_COMMON_PROPERTY_H

#include <optional>
#include <string>

namespace odr::common {

class Property {
public:
  virtual ~Property() = default;

  virtual std::optional<std::string> value() const = 0;

  virtual void set(std::optional<std::string> value) const = 0;
};

} // namespace odr::common

#endif // ODR_COMMON_PROPERTY_H
