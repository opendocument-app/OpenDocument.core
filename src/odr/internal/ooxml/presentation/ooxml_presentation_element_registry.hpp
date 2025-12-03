#pragma once

#include <odr/document_element.hpp>
#include <odr/document_element_identifier.hpp>

#include <map>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

class ElementRegistry final {
public:
  struct Element final {
    ExtendedElementIdentifier parent_id;
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

  ExtendedElementIdentifier create_element();
  Table &create_table_element(ExtendedElementIdentifier id);
  Text &create_text_element(ExtendedElementIdentifier id);

  [[nodiscard]] Element &element(ExtendedElementIdentifier id);
  [[nodiscard]] const Element &element(ExtendedElementIdentifier id) const;

  [[nodiscard]] Table &table_element(ExtendedElementIdentifier id);
  [[nodiscard]] const Table &table_element(ExtendedElementIdentifier id) const;

  [[nodiscard]] Text &text_element(ExtendedElementIdentifier id);
  [[nodiscard]] const Text &text_element(ExtendedElementIdentifier id) const;

  void append_child(ExtendedElementIdentifier parent_id,
                    ExtendedElementIdentifier child_id);
  void append_column(ExtendedElementIdentifier table_id,
                     ExtendedElementIdentifier column_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, Text> m_texts;

  void check_element_id(ExtendedElementIdentifier id) const;
  void check_table_id(ExtendedElementIdentifier id) const;
  void check_text_id(ExtendedElementIdentifier id) const;
};

} // namespace odr::internal::ooxml::presentation
