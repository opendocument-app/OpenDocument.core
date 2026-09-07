#pragma once

#include <odr/definitions.hpp>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {
class ElementRegistry;

ElementIdentifier parse_tree(ElementRegistry &registry, pugi::xml_node node);

/// Rebuilds the row and cell index of @p sheet_id off its dom, for a write
/// that split a repeat. A cell node keeps the element it already carries.
void reindex_sheet(ElementRegistry &registry, ElementIdentifier sheet_id);

} // namespace odr::internal::odf
