#include <odr/internal/util/document_util.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>

#include <odr/internal/abstract/document.hpp>

#include <algorithm>
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
  for (ElementHandle current =
           element_adapter.element_previous_sibling(element_id);
       !current.is_null(); current = current.adapter().element_previous_sibling(
                               current.identifier)) {
    ++distance;
  }

  return DocumentPath::Child(distance);
}

ElementHandle
navigate_path_component(const abstract::ElementAdapter &element_adapter,
                        const ElementIdentifier element_id,
                        const DocumentPath::Component &component) {
  if (const auto *child = std::get_if<DocumentPath::Child>(&component);
      child != nullptr) {
    ElementHandle result = element_adapter.element_first_child(element_id);
    if (result.is_null()) {
      throw std::invalid_argument("child not found");
    }
    for (std::uint32_t i = 0; i < child->number(); ++i) {
      result = result.adapter().element_next_sibling(result.identifier);
      if (result.is_null()) {
        throw std::invalid_argument("child not found");
      }
    }
    return result;
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

  for (ElementHandle current = {&element_adapter, to_element_id};
       current.identifier != from_element_id;) {
    const ElementHandle parent =
        current.adapter().element_parent(current.identifier);
    if (parent.is_null()) {
      break;
    }
    if (current.is_null()) {
      throw std::invalid_argument(
          "Element is not a descendant of the specified root.");
    }

    const DocumentPath::Component component =
        extract_path_component(element_adapter, current.identifier);
    reverse.push_back(component);

    current = parent;
  }

  std::ranges::reverse(reverse);
  return DocumentPath(std::move(reverse));
}

ElementHandle
document::navigate_path(const abstract::ElementAdapter &element_adapter,
                        const ElementIdentifier from_element_id,
                        const DocumentPath &path) {
  ElementHandle current{element_adapter, from_element_id};
  for (const DocumentPath::Component &component : path) {
    current = navigate_path_component(current.adapter(), current.identifier,
                                      component);
  }
  return current;
}

} // namespace odr::internal::util
