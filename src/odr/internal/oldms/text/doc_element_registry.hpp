#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <cstdint>
#include <string>
#include <tuple>

namespace odr::internal::oldms::text {

class ElementRegistry final
    : public internal::ElementRegistry<ElementNode<ElementIdentifier>> {
public:
  struct Text final {
    std::string text;
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();

  [[nodiscard]] auto &text_element_at(this auto &self,
                                      const ElementIdentifier id) {
    return self.m_texts.at(id);
  }

  /// Character style of a span or paragraph element, as an index into the
  /// document's `StyleRegistry` (0 is the default style).
  void set_element_style_index(ElementIdentifier id, std::uint32_t index);
  [[nodiscard]] std::uint32_t element_style_index(ElementIdentifier id) const;

private:
  SideTable<Text> m_texts;
  SideTable<std::uint32_t> m_style_indices;
};

} // namespace odr::internal::oldms::text
