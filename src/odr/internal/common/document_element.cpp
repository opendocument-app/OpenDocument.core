#include <odr/internal/common/document_element.hpp>
#include <stdexcept>

namespace odr::internal::common {

std::pair<abstract::Element *, ElementIdentifier>
Element::parent(const abstract::Document *, ElementIdentifier) const {
  return {m_parent, 0}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Element::first_child(const abstract::Document *, ElementIdentifier) const {
  return {m_first_child, 0}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Element::last_child(const abstract::Document *, ElementIdentifier) const {
  return {m_last_child, 0}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Element::previous_sibling(const abstract::Document *, ElementIdentifier) const {
  return {m_previous_sibling, 0}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Element::next_sibling(const abstract::Document *, ElementIdentifier) const {
  return {m_next_sibling, 0};
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

std::pair<abstract::Element *, ElementIdentifier>
Table::first_child(const abstract::Document *, ElementIdentifier) const {
  return {};
}

std::pair<abstract::Element *, ElementIdentifier>
Table::last_child(const abstract::Document *, ElementIdentifier) const {
  return {};
}

std::pair<abstract::Element *, ElementIdentifier>
Table::first_column(const abstract::Document *, ElementIdentifier) const {
  return {m_first_column, 0}; // TODO
}

std::pair<abstract::Element *, ElementIdentifier>
Table::first_row(const abstract::Document *, ElementIdentifier) const {
  return {m_first_child, 0}; // TODO
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

void Sheet::init_child(Element *element) { element->m_parent = this; }

} // namespace odr::internal::common
