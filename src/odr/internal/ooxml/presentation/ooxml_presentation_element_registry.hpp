#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <tuple>

#include <pugixml.hpp>

namespace odr::internal::ooxml::presentation {

struct RegistryElement final : ElementNode<ElementIdentifier> {
  pugi::xml_node node;
};

class ElementRegistry final
    : public internal::ElementRegistry<RegistryElement> {
public:
  struct Table final {
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  struct Text final {
    pugi::xml_node last;
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type,
                                                          pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Table &>
  create_table_element(pugi::xml_node node);
  std::tuple<ElementIdentifier, Element &, Text &>
  create_text_element(pugi::xml_node first_node, pugi::xml_node last_node);

  [[nodiscard]] Table &table_element_at(const ElementIdentifier id) {
    return m_tables.at(id);
  }
  [[nodiscard]] Text &text_element_at(const ElementIdentifier id) {
    return m_texts.at(id);
  }

  [[nodiscard]] const Table &
  table_element_at(const ElementIdentifier id) const {
    return m_tables.at(id);
  }
  [[nodiscard]] const Text &text_element_at(const ElementIdentifier id) const {
    return m_texts.at(id);
  }

  void append_column(ElementIdentifier table_id, ElementIdentifier column_id);

private:
  SideTable<Table> m_tables;
  SideTable<Text> m_texts;
};

} // namespace odr::internal::ooxml::presentation
