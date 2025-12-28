#include <odr/internal/util/document_util.hpp>

#include <odr/document_path.hpp>

#include <odr/internal/abstract/document_element.hpp>

#include <ranges>
#include <stdexcept>
#include <vector>

namespace odr::internal::util {

namespace {

DocumentPath::Component
extract_path_component(const abstract::ElementAdapter &element_adapter,
                       const ElementIdentifier element_id) {
  if (element_adapter.element_type(element_id) == ElementType::sheet_cell) {
    const abstract::SheetCellAdapter *sheet_cell_adapter =
        element_adapter.sheet_cell_adapter(element_id);
    if (sheet_cell_adapter == nullptr) {
      throw std::invalid_argument("Sheet cell adapter not found.");
    }
    return DocumentPath::Cell(
        sheet_cell_adapter->sheet_cell_position(element_id));
  }

  std::uint32_t distance = 0;
  for (ElementIdentifier current_id =
           element_adapter.element_previous_sibling(element_id);
       current_id != null_element_id;
       current_id = element_adapter.element_previous_sibling(current_id)) {
    ++distance;
  }

  return DocumentPath::Child(distance);
}

ElementIdentifier
navigate_path_component(const abstract::ElementAdapter &element_adapter,
                        const ElementIdentifier element_id,
                        const DocumentPath::Component &component) {
  if (const auto *child = std::get_if<DocumentPath::Child>(&component);
      child != nullptr) {
    ElementIdentifier result_id =
        element_adapter.element_first_child(element_id);
    if (result_id == null_element_id) {
      throw std::invalid_argument("child not found");
    }
    for (std::uint32_t i = 0; i < child->number(); ++i) {
      result_id = element_adapter.element_next_sibling(result_id);
      if (result_id == null_element_id) {
        throw std::invalid_argument("child not found");
      }
    }
    return result_id;
  }

  if (const auto *cell = std::get_if<DocumentPath::Cell>(&component);
      cell != nullptr) {
    const abstract::SheetAdapter *sheet_adapter =
        element_adapter.sheet_adapter(element_id);
    if (sheet_adapter == nullptr) {
      throw std::invalid_argument("sheet adapter not found");
    }
    return sheet_adapter->sheet_cell(element_id, cell->position().column(),
                                     cell->position().row());
  }

  throw std::invalid_argument("unknown document path component");
}

} // namespace

DocumentPath
document::extract_path(const abstract::ElementAdapter &element_adapter,
                       const ElementIdentifier to_element_id,
                       const ElementIdentifier from_element_id) {
  if (to_element_id == null_element_id) {
    throw std::invalid_argument("Element identifier cannot be null.");
  }

  std::vector<DocumentPath::Component> reverse;

  for (ElementIdentifier current_id = to_element_id;
       current_id != from_element_id;
       current_id = element_adapter.element_parent(current_id)) {
    if (current_id == null_element_id) {
      throw std::invalid_argument(
          "Element is not a descendant of the specified root.");
    }

    const DocumentPath::Component component =
        extract_path_component(element_adapter, current_id);
    reverse.push_back(component);
  }

  std::ranges::reverse(reverse);
  return DocumentPath(std::move(reverse));
}

ElementIdentifier
document::navigate_path(const abstract::ElementAdapter &element_adapter,
                        const ElementIdentifier from_element_id,
                        const DocumentPath &path) {
  ElementIdentifier current_id = from_element_id;
  for (const DocumentPath::Component &component : path) {
    current_id =
        navigate_path_component(element_adapter, current_id, component);
  }
  return current_id;
}

} // namespace odr::internal::util
