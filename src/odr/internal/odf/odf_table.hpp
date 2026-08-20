#pragma once

#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// The `<table:table-row>` children of a table, in document order, including
/// those a grouping element holds — `<table:table-header-rows>`,
/// `<table:table-rows>` and `<table:table-row-group>`, which nest
/// ([ODF 1.2] 9.1.7).
[[nodiscard]] std::vector<pugi::xml_node> table_rows(pugi::xml_node table);

/// The `<table:table-column>` children of a table, in document order,
/// including those a grouping element holds — `<table:table-header-columns>`,
/// `<table:table-columns>` and `<table:table-column-group>`, which nest
/// ([ODF 1.2] 9.1.6).
[[nodiscard]] std::vector<pugi::xml_node> table_columns(pugi::xml_node table);

} // namespace odr::internal::odf
