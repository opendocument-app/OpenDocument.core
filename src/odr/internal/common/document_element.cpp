#include <odr/internal/common/document_element.hpp>
#include <stdexcept>

namespace odr::internal::common {

Element::Element(pugi::xml_node node) : m_node{node} {
  if (!node) {
    // TODO log error
    throw std::runtime_error("node not set");
  }
}

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

Table::Table(pugi::xml_node node) : Element(node) {}

Element *Table::first_child(const abstract::Document *) const { return {}; }

Element *Table::last_child(const abstract::Document *) const { return {}; }

Element *Table::first_column(const abstract::Document * /*document*/) const {
  return m_first_column;
}

Element *Table::first_row(const abstract::Document * /*document*/) const {
  return m_first_child;
}

void Table::init_append_column(Element *element) {
  element->m_previous_sibling = m_last_column;
  element->m_parent = this;
  if (m_last_column == nullptr) {
    m_first_column = element;
  } else {
    m_last_column->m_next_sibling = element;
  }
  m_last_column = element;
}

void Table::init_append_row(Element *element) {
  Element::init_append_child(element);
}

Sheet::Sheet(pugi::xml_node node) : Element(node) {}

void Sheet::init_child(Element *element) { element->m_parent = this; }

} // namespace odr::internal::common
