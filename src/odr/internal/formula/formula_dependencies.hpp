#pragma once

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_ast.hpp>

#include <optional>
#include <string>
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
  /// False where the formula names something no position can be read out of —
  /// a name, or a reference over several sheets. It may then read anything.
  bool complete{true};
};

/// The cells @p node reads. A reference into another document is stated with
/// its document, and nothing here opens one.
[[nodiscard]] References references(const Node &node);

} // namespace odr::internal::formula
