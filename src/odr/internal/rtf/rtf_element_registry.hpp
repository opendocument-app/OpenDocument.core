#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <string>
#include <tuple>

namespace odr::internal::rtf {

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

private:
  SideTable<Text> m_texts;
};

} // namespace odr::internal::rtf
