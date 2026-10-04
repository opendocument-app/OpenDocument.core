#pragma once

#include <odr/sheet_position.hpp>

#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

/// What a recalculation did, each list sorted.
struct SheetRecalculation final {
  /// The formula cells whose result changed, or that had none before.
  std::vector<SheetPosition> changed;
  /// The cells of a cycle, which get no result.
  std::vector<SheetPosition> circular;
  /// The stale formula cells the evaluator has no answer for.
  std::vector<SheetPosition> unevaluated;
};

/// Recalculates stale formulas and writes their results. See decision 32 in
/// `docs/design/spreadsheet-editing.md` for invalidation rules.
SheetRecalculation recalculate(const abstract::Document &document);

/// Whether an edit since the last recalculation left a formula stale.
[[nodiscard]] bool is_edited(const abstract::Document &document);

} // namespace odr::internal
