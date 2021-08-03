#ifndef ODR_INTERNAL_COMMON_ELEMENT_ATTRIBUTES_STORE_H
#define ODR_INTERNAL_COMMON_ELEMENT_ATTRIBUTES_STORE_H

#include <internal/identifier.h>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace odr::internal {

template <typename Attribute> class DenseElementAttributeStore {
public:
  Attribute &operator[](const ElementIdentifier element_id) {
    check_id_(element_id);
    return m_elements[element_id.id - 1];
  }

  const Attribute &operator[](const ElementIdentifier element_id) const {
    check_id_(element_id);
    return m_elements[element_id.id - 1];
  }

  ElementIdentifier push_back(const Attribute &element) {
    m_elements.push_back(element);
    return m_elements.size();
  }

  template <typename... Args>
  ElementIdentifier emplace_back(const Args &&... args) {
    m_elements.emplace_back(std::forward<Args>(args)...);
    return m_elements.size();
  }

  std::size_t size() const { return m_elements.size(); }

private:
  std::vector<Attribute> m_elements;

  void check_id_(const ElementIdentifier element_id) const {
    if (!element_id) {
      throw std::invalid_argument("identifier is null");
    }
    if (element_id.id >= m_elements.size()) {
      throw std::invalid_argument("identifier out of range");
    }
  }
};

template <typename Attribute>
using SparseElementAttributeStore =
    std::unordered_map<ElementIdentifier, Attribute>;

} // namespace odr::internal

#endif // ODR_INTERNAL_COMMON_ELEMENT_ATTRIBUTES_STORE_H
