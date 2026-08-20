#pragma once

#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// A table's rows in document order, including those a grouping element holds
/// - the `table:table-*-rows` family, which nests ([ODF 1.2] 9.1.7).
[[nodiscard]] std::vector<pugi::xml_node> table_rows(pugi::xml_node table);

/// The column counterpart of `table_rows()` ([ODF 1.2] 9.1.6).
[[nodiscard]] std::vector<pugi::xml_node> table_columns(pugi::xml_node table);

} // namespace odr::internal::odf
