#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <any>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr {
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::odf {

class Element final {
public:
  Element();
  explicit Element(pugi::xml_node node);

  explicit operator bool() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const;

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const;

private:
  class Adapter {
  public:
    virtual ~Adapter() = default;

    [[nodiscard]] virtual ElementType type(pugi::xml_node node) const = 0;

    [[nodiscard]] virtual Element parent(pugi::xml_node node) const = 0;
    [[nodiscard]] virtual Element first_child(pugi::xml_node node) const = 0;
    [[nodiscard]] virtual Element
    previous_sibling(pugi::xml_node node) const = 0;
    [[nodiscard]] virtual Element next_sibling(pugi::xml_node node) const = 0;

    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(pugi::xml_node node) const = 0;

    virtual void update_properties(
        pugi::xml_node node,
        std::unordered_map<ElementProperty, std::any> properties) const = 0;
  };
  class DefaultAdapter;

  static Adapter *default_adapter(pugi::xml_node node);

  Element(pugi::xml_node node, Adapter *adapter);

  pugi::xml_node m_node;
  Adapter *m_adapter{nullptr};
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
