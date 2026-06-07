#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <map>
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

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
};

} // namespace odr::internal::oldms::text
