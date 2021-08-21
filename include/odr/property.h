#ifndef ODR_PROPERTY_H
#define ODR_PROPERTY_H

#include <any>
#include <cstdint>
#include <string>

namespace odr {

class PropertyValue {
public:
  virtual ~PropertyValue() = default;

  virtual explicit operator bool() const = 0;

  [[nodiscard]] virtual std::any get() const = 0;
  [[nodiscard]] std::string get_string() const;
  [[nodiscard]] std::uint32_t get_uint32() const;
  [[nodiscard]] bool get_bool() const;
};

} // namespace odr

#endif // ODR_PROPERTY_H
