#ifndef ODR_PROPERTY_H
#define ODR_PROPERTY_H

#include <memory>
#include <optional>
#include <string>

namespace odr::internal::abstract {
class Property;
}

namespace odr {

class Property final {
public:
  Property();
  explicit Property(std::shared_ptr<internal::abstract::Property> impl);

  bool operator==(const Property &rhs) const;
  bool operator!=(const Property &rhs) const;
  std::string operator*() const;
  explicit operator bool() const;
  explicit operator const char *() const;
  explicit operator std::string() const;

  bool readonly() const noexcept;
  bool optional() const noexcept;

  std::optional<std::string> value() const;

  void set(std::optional<std::string> value) const;

private:
  std::shared_ptr<internal::abstract::Property> m_impl;
};

} // namespace odr

#endif // ODR_PROPERTY_H
