#include <odr/internal/oldms/text/doc_element_registry.hpp>

namespace odr::internal::oldms::text {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  return create_element_(type);
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element_(ElementType::text);
  Text &text = m_texts.emplace(element_id, Text{});
  return {element_id, element, text};
}

void ElementRegistry::set_element_style_index(const ElementIdentifier id,
                                              const std::uint32_t index) {
  check_element_id(id);
  m_style_indices.emplace(id, index);
}

std::uint32_t
ElementRegistry::element_style_index(const ElementIdentifier id) const {
  const std::uint32_t *index = m_style_indices.find(id);
  return index != nullptr ? *index : 0;
}

} // namespace odr::internal::oldms::text
