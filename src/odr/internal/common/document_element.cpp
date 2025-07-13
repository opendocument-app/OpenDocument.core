#include <odr/internal/common/document_element.hpp>

namespace odr::internal {

abstract::Element *Element::parent(const abstract::Document *) const {
  return m_parent;
}

abstract::Element *Element::first_child(const abstract::Document *) const {
  return m_first_child;
}

abstract::Element *Element::last_child(const abstract::Document *) const {
  return m_last_child;
}

abstract::Element *Element::previous_sibling(const abstract::Document *) const {
  return m_previous_sibling;
}

abstract::Element *Element::next_sibling(const abstract::Document *) const {
  return m_next_sibling;
}

void Element::append_child_(Element *element) {
  element->m_previous_sibling = m_last_child;
  element->m_parent = this;
  if (m_last_child == nullptr) {
    m_first_child = element;
  } else {
    m_last_child->m_next_sibling = element;
  }
  m_last_child = element;
}

abstract::Element *Table::first_column(const abstract::Document *) const {
  return m_first_column;
}

abstract::Element *Table::first_row(const abstract::Document *) const {
  return m_first_child;
}

void Table::append_column_(Element *element) {
  element->m_previous_sibling = m_last_column;
  element->m_parent = this;
  if (m_last_column == nullptr) {
    m_first_column = element;
  } else {
    m_last_column->m_next_sibling = element;
  }
  m_last_column = element;
}

void Table::append_row_(Element *element) { Element::append_child_(element); }

} // namespace odr::internal
