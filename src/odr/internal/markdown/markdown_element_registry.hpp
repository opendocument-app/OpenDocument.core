#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace odr::internal::markdown {

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

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Link &> create_link_element();
  std::tuple<ElementIdentifier, Element &, List &> create_list_element();
  std::tuple<ElementIdentifier, Element &, ListItem &>
  create_list_item_element();
  std::tuple<ElementIdentifier, Element &, Table &> create_table_element();
  std::tuple<ElementIdentifier, Element &, TableCell &>
  create_table_cell_element();

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Text &text_element_at(ElementIdentifier id);
  [[nodiscard]] Table &table_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Text &text_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Link &link_element_at(ElementIdentifier id) const;
  [[nodiscard]] const List &list_element_at(ElementIdentifier id) const;
  [[nodiscard]] const ListItem &
  list_item_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Table &table_element_at(ElementIdentifier id) const;
  [[nodiscard]] const TableCell &
  table_cell_element_at(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);
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
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Link> m_links;
  std::unordered_map<ElementIdentifier, List> m_lists;
  std::unordered_map<ElementIdentifier, ListItem> m_list_items;
  std::unordered_map<ElementIdentifier, Table> m_tables;
  std::unordered_map<ElementIdentifier, TableCell> m_table_cells;
  std::unordered_map<ElementIdentifier, std::uint32_t> m_text_style_indices;
  std::unordered_map<ElementIdentifier, std::uint32_t>
      m_paragraph_style_indices;

  void check_element_id(ElementIdentifier id) const;

  /// Links @p child_id onto the chain @p first_id / @p last_id delimit.
  void link_child(ElementIdentifier parent_id, ElementIdentifier child_id,
                  ElementIdentifier &first_id, ElementIdentifier &last_id);
};

} // namespace odr::internal::markdown
