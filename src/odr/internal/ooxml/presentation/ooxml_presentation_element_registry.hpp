#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <map>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

class ElementRegistry final {
public:
  struct Element final {
    ElementIdentifier parent_id{null_element_id};
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
    pugi::xml_node node;
    bool is_editable{false};
  };

  struct Table final {
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  struct Text final {
    pugi::xml_node last;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type,
                                                          pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Table &>
  create_table_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Text &>
  create_text_element(pugi::xml_node first_node, pugi::xml_node last_node);

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Table &table_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Table &table_element_at(ElementIdentifier id) const;

  [[nodiscard]] Element *element(ElementIdentifier id);
  [[nodiscard]] Table *table_element(ElementIdentifier id);
  [[nodiscard]] Text *text_element(ElementIdentifier id);

  [[nodiscard]] const Element *element(ElementIdentifier id) const;
  [[nodiscard]] const Table *table_element(ElementIdentifier id) const;
  [[nodiscard]] const Text *text_element(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);
  void append_column(ElementIdentifier table_id, ElementIdentifier column_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, Text> m_texts;

  void check_element_id(ElementIdentifier id) const;
  void check_table_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
};

} // namespace odr::internal::ooxml::presentation
