#ifndef ODR_INTERNAL_COMMON_ELEMENT_REGISTRY_H
#define ODR_INTERNAL_COMMON_ELEMENT_REGISTRY_H

#include <internal/identifier.h>
#include <stdexcept>
#include <vector>

namespace odr::internal {

template <typename Element> class ElementRegistry {
public:
  Element &operator[](const ElementIdentifier element_id) {
    check_id_(element_id);
    return m_elements[element_id.id - 1];
  }

  const Element &operator[](const ElementIdentifier element_id) const {
    check_id_(element_id);
    return m_elements[element_id.id - 1];
  }

  ElementIdentifier new_element(const Element &element) {
    m_elements.push_back(element);
    return m_elements.size();
  }

private:
  std::vector<Element> m_elements;

  void check_id_(const ElementIdentifier element_id) const {
    if (!element_id) {
      throw invalid_argument("identifier is null");
    }
    if (element_id.id >= m_elements.size()) {
      throw invalid_argument("identifier out of range");
    }
  }
};

} // namespace odr::internal

#endif // ODR_INTERNAL_COMMON_ELEMENT_REGISTRY_H
