#include <odr/internal/common/document_element.hpp>

namespace odr::internal::common {

Element::Element() = default;

Element::Element(const pugi::xml_node node) : m_node{node} {}

Element *Element::parent(const abstract::Document *) const { return m_parent; }

Element *Element::first_child(const abstract::Document *) const {
  return m_first_child;
}

Element *Element::previous_sibling(const abstract::Document *) const {
  return m_previous_sibling;
}

Element *Element::next_sibling(const abstract::Document *) const {
  return m_next_sibling;
}

} // namespace odr::internal::common
