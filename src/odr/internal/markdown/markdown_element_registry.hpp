#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/common/element_registry.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

namespace odr::internal::markdown {

class ElementRegistry final
    : public internal::ElementRegistry<ElementNode<ElementIdentifier>> {
public:
  struct Text final {
    std::string text;
  };

  struct Link final {
    std::string href;
  };

  struct List final {
    ListType type{ListType::unordered};
  };

  struct ListItem final {
    std::string marker;
    std::optional<std::uint32_t> number;
  };

  /// The columns hang off the table on a chain of their own, as they do in
  /// odf: the rows are the table's children, and one sibling chain cannot
  /// carry both.
  struct Table final {
    TableDimensions dimensions;
    ElementIdentifier first_column_id{null_element_id};
    ElementIdentifier last_column_id{null_element_id};
  };

  struct TableCell final {
    std::optional<HorizontalAlign> horizontal_align;
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Link &> create_link_element();
  std::tuple<ElementIdentifier, Element &, List &> create_list_element();
  std::tuple<ElementIdentifier, Element &, ListItem &>
  create_list_item_element();
  std::tuple<ElementIdentifier, Element &, Table &> create_table_element();
  std::tuple<ElementIdentifier, Element &, TableCell &>
  create_table_cell_element();

  [[nodiscard]] Text &text_element_at(const ElementIdentifier id) {
    return m_texts.at(id);
  }
  [[nodiscard]] Table &table_element_at(const ElementIdentifier id) {
    return m_tables.at(id);
  }

  [[nodiscard]] const Text &text_element_at(const ElementIdentifier id) const {
    return m_texts.at(id);
  }
  [[nodiscard]] const Link &link_element_at(const ElementIdentifier id) const {
    return m_links.at(id);
  }
  [[nodiscard]] const List &list_element_at(const ElementIdentifier id) const {
    return m_lists.at(id);
  }
  [[nodiscard]] const ListItem &
  list_item_element_at(const ElementIdentifier id) const {
    return m_list_items.at(id);
  }
  [[nodiscard]] const Table &
  table_element_at(const ElementIdentifier id) const {
    return m_tables.at(id);
  }
  [[nodiscard]] const TableCell &
  table_cell_element_at(const ElementIdentifier id) const {
    return m_table_cells.at(id);
  }

  void append_column(ElementIdentifier table_id, ElementIdentifier column_id);

  /// Character style of an element, as an index into the document's
  /// `StyleRegistry` (0 is the default style).
  void set_element_text_style_index(ElementIdentifier id, std::uint32_t index);
  [[nodiscard]] std::uint32_t
  element_text_style_index(ElementIdentifier id) const;

  /// Paragraph style of an element, indexed the same way.
  void set_element_paragraph_style_index(ElementIdentifier id,
                                         std::uint32_t index);
  [[nodiscard]] std::uint32_t
  element_paragraph_style_index(ElementIdentifier id) const;

private:
  SideTable<Text> m_texts;
  SideTable<Link> m_links;
  SideTable<List> m_lists;
  SideTable<ListItem> m_list_items;
  SideTable<Table> m_tables;
  SideTable<TableCell> m_table_cells;
  SideTable<std::uint32_t> m_text_style_indices;
  SideTable<std::uint32_t> m_paragraph_style_indices;
};

} // namespace odr::internal::markdown
