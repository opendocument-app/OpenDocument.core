#pragma once

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_ast.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::formula {

/// The rectangle one reference names. An axis the reference leaves out
/// reaches the whole sheet, which is what `A:A` and `1:1` mean.
struct Extent final {
  std::optional<std::string> document{};
  std::optional<std::string> sheet{};
  TableRange range{};

  friend bool operator==(const Extent &, const Extent &) = default;
};

/// What a formula reads.
struct References final {
  std::vector<Extent> extents;
  /// False for unresolved names, dynamic references or ranges across sheets.
  /// Such a formula may read any cell.
  bool complete{true};
};

/// The cells @p node reads. A reference into another document is stated with
/// its document, and nothing here opens one.
[[nodiscard]] References references(const Node &node);

/// What @p node reads, as an editable sheet page states it in
/// `data-odr-reads`: space-separated `sheet,first column,last column,first
/// row,last row` boxes, `*` for an open end. @p sheet_of gives the ordinal of
/// a sheet by its name, read without case, and @p name_of what a name stands
/// for. A formula that may read any cell, or that names a sheet or a name
/// they do not know, reads all of @p own_sheet.
[[nodiscard]] std::string spell_reads(
    const Node &node, std::uint32_t own_sheet,
    const std::function<std::optional<std::uint32_t>(std::string_view)>
        &sheet_of,
    const std::function<std::optional<Node>(std::string_view)> &name_of);

} // namespace odr::internal::formula
