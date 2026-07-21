#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal::oldms::text {

class ElementRegistry final {
public:
  struct Element final {
    ElementIdentifier parent_id{null_element_id};
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
  };

  struct Text final {
    std::string text;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Text &text_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Text &text_element_at(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);

  /// Character style of a span or paragraph element, as an index into the
  /// document's `StyleRegistry` (0 is the default style).
  void set_element_style_index(ElementIdentifier id, std::uint32_t index);
  [[nodiscard]] std::uint32_t element_style_index(ElementIdentifier id) const;

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, std::uint32_t> m_style_indices;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
};

} // namespace odr::internal::oldms::text
