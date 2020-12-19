#ifndef ODR_PROPERTY_H
#define ODR_PROPERTY_H

#include <memory>
#include <optional>
#include <string>

namespace odr::common {
class Property;
}

namespace odr {

class Property final {
public:
  explicit Property(std::shared_ptr<const common::Property> impl);

  bool operator==(const Property &rhs) const;
  bool operator!=(const Property &rhs) const;
  explicit operator bool() const;

  std::optional<std::string> value() const;

  void set(std::optional<std::string> value) const;

private:
  std::shared_ptr<const common::Property> m_impl;
};

} // namespace odr

#endif // ODR_PROPERTY_H
