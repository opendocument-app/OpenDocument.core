#pragma once

#include <functional>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

using TableNodeVisitor = std::function<void(pugi::xml_node)>;

/// Calls @p visit for a table's rows in document order, including those a
/// grouping element holds - the `table:table-*-rows` family, which nests
/// ([ODF 1.2] 9.1.7). A visitor, not a container: a sheet is walked repeatedly
/// and each walk would materialise every row.
void for_each_table_row(pugi::xml_node table, const TableNodeVisitor &visit);

/// The column counterpart of @ref for_each_table_row ([ODF 1.2] 9.1.6).
void for_each_table_column(pugi::xml_node table, const TableNodeVisitor &visit);

} // namespace odr::internal::odf
