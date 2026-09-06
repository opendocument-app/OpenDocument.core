#include <odr/internal/rtf/rtf_element_registry.hpp>

namespace odr::internal::rtf {

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

} // namespace odr::internal::rtf
