#pragma once

#include <odr/document_element.hpp>
#include <odr/document_element_identifier.hpp>

#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {

class ElementRegistry {
public:
  struct Entry {
    ExtendedElementIdentifier parent_id;
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
    pugi::xml_node node;
    bool is_editable{false};
  };

  struct Table {
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  struct Text {
    pugi::xml_node last;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  ElementIdentifier create_element();
  Table &create_table(ElementIdentifier id);
  Text &create_text(ElementIdentifier id);

  [[nodiscard]] Entry &entry(ElementIdentifier id);
  [[nodiscard]] const Entry &entry(ElementIdentifier id) const;

  [[nodiscard]] Table &table(ElementIdentifier id);
  [[nodiscard]] const Table &table(ElementIdentifier id) const;

  [[nodiscard]] Text &text(ElementIdentifier id);
  [[nodiscard]] const Text &text(ElementIdentifier id) const;

private:
  std::vector<Entry> m_entries;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, Text> m_texts;

  void check_element_id(ElementIdentifier id) const;
  void check_table_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
};

} // namespace odr::internal::odf
