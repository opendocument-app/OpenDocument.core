#ifndef ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H

#include <odr/internal/abstract/document_element.hpp>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class Document;
class Element;
} // namespace odr::internal::abstract

namespace odr::internal::common {

class Element : public virtual abstract::Element {
public:
  Element() = default;

  explicit Element(const pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] Element *parent(const abstract::Document *) const final {
    return m_parent;
  }

  [[nodiscard]] Element *first_child(const abstract::Document *) const final {
    return m_first_child;
  }

  [[nodiscard]] Element *
  previous_sibling(const abstract::Document *) const final {
    return m_previous_sibling;
  }

  [[nodiscard]] Element *next_sibling(const abstract::Document *) const final {
    return m_next_sibling;
  }

protected:
  pugi::xml_node m_node;

  Element *m_parent;
  Element *m_first_child;
  Element *m_previous_sibling;
  Element *m_next_sibling;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
