#ifndef ODR_INTERNAL_ABSTRACT_ELEMENT_H
#define ODR_INTERNAL_ABSTRACT_ELEMENT_H

#include <any>
#include <unordered_map>

namespace odr {
enum class ElementType;
enum class ElementProperty;

class Element;
class Table;
} // namespace odr

namespace odr::internal::abstract {

class Element {
public:
  virtual ~Element() = default;

  virtual bool operator==(const Element &rhs) const = 0;
  virtual bool operator!=(const Element &rhs) const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;

  [[nodiscard]] virtual odr::Table table() const = 0;

  [[nodiscard]] virtual odr::Element parent() const = 0;
  [[nodiscard]] virtual odr::Element first_child() const = 0;
  [[nodiscard]] virtual odr::Element previous_sibling() const = 0;
  [[nodiscard]] virtual odr::Element next_sibling() const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties() const = 0;

  virtual void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_ELEMENT_H
