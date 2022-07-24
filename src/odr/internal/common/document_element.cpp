#include <odr/internal/common/document_element.hpp>

namespace odr::internal::common {

Element::Element() = default;

Element::Element(const pugi::xml_node node) : m_node{node} {}

Element *Element::parent(const abstract::Document *) const { return m_parent; }

Element *Element::first_child(const abstract::Document *) const {
  return m_first_child;
}

Element *Element::last_child(const abstract::Document *) const {
  return m_last_child;
}

Element *Element::previous_sibling(const abstract::Document *) const {
  return m_previous_sibling;
}

Element *Element::next_sibling(const abstract::Document *) const {
  return m_next_sibling;
}

void Element::init_append_child(Element *element) {
  element->m_previous_sibling = m_last_child;
  element->m_parent = this;
  if (m_last_child == nullptr) {
    m_first_child = element;
  } else {
    m_last_child->m_next_sibling = element;
  }
  m_last_child = element;
}

} // namespace odr::internal::common
