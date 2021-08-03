#include <internal/common/document.h>
#include <odr/document.h>

namespace odr::internal::common {

ElementIdentifier
Document::new_element_(const ElementType type, const ElementIdentifier parent,
                       const ElementIdentifier previous_sibling) {
  Element element;
  element.type = type;
  element.parent = parent;
  element.previous_sibling = previous_sibling;

  auto result = m_elements.push_back(element);

  if (parent && !previous_sibling) {
    m_elements[parent].first_child = result;
  }
  if (previous_sibling) {
    m_elements[previous_sibling].next_sibling = result;
  }

  return result;
}

ElementIdentifier Document::root_element() const { return m_root; }

ElementIdentifier Document::first_entry_element() const {
  return element_first_child(m_root);
}

ElementType Document::element_type(const ElementIdentifier element_id) const {
  return m_elements[element_id].type;
}

ElementIdentifier
Document::element_parent(const ElementIdentifier element_id) const {
  return m_elements[element_id].parent;
}

ElementIdentifier
Document::element_first_child(const ElementIdentifier element_id) const {
  return m_elements[element_id].first_child;
}

ElementIdentifier
Document::element_previous_sibling(const ElementIdentifier element_id) const {
  return m_elements[element_id].previous_sibling;
}

ElementIdentifier
Document::element_next_sibling(const ElementIdentifier element_id) const {
  return m_elements[element_id].next_sibling;
}

std::shared_ptr<abstract::Table>
Document::table(const ElementIdentifier element_id) const {
  return m_tables.at(element_id);
}

} // namespace odr::internal::common
