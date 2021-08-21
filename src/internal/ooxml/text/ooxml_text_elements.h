#ifndef ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H
#define ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H

#include <any>
#include <memory>
#include <odr/document.h>
#include <odr/element.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

class ElementAdapter;

class Element;

using ElementRange = ElementRangeTemplate<Element>;

class Element final {
public:
  Element();
  explicit Element(pugi::xml_node node);
  Element(pugi::xml_node node, ElementAdapter *adapter);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] pugi::xml_node xml_node() const;
  [[nodiscard]] ElementAdapter *adapter() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const;

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const;

private:
  pugi::xml_node m_node;
  ElementAdapter *m_adapter{nullptr};
};

class ElementAdapter {
public:
  static ElementAdapter *default_adapter(pugi::xml_node node);

  virtual ~ElementAdapter() = default;

  [[nodiscard]] virtual ElementType type(pugi::xml_node node) const = 0;

  [[nodiscard]] virtual Element parent(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element first_child(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element previous_sibling(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element next_sibling(pugi::xml_node node) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties(pugi::xml_node node) const = 0;

  virtual void update_properties(
      pugi::xml_node node,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_ELEMENTS_H
