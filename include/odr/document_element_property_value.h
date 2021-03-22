#ifndef ODR_ELEMENT_PROPERTY_VALUE_H
#define ODR_ELEMENT_PROPERTY_VALUE_H

#include <cstdint>
#include <memory>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr {
class Element;
enum class ElementProperty;
class PageStyle;

class ElementPropertyValue {
public:
  bool operator==(const ElementPropertyValue &rhs) const;
  bool operator!=(const ElementPropertyValue &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] std::any get() const;
  [[nodiscard]] std::string get_string() const;
  [[nodiscard]] std::uint32_t get_uint32() const;
  [[nodiscard]] bool get_bool() const;
  [[nodiscard]] const char *get_optional_string() const;

  void set(const std::any &value) const;
  void set_string(const std::string &value) const;

  void remove() const;

private:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};
  ElementProperty m_property;

  ElementPropertyValue();
  ElementPropertyValue(std::shared_ptr<const internal::abstract::Document> impl,
                       std::uint64_t id, ElementProperty property);

  friend Element;
  friend PageStyle;
};

} // namespace odr

#endif // ODR_ELEMENT_PROPERTY_VALUE_H
