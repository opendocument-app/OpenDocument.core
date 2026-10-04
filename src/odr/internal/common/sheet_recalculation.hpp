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

/// Computes the stale formula cells of @p document and writes each result
/// into the file (decision 32 of `docs/design/spreadsheet-editing.md`). A
/// cell is stale where an edit since the last recalculation reaches it,
/// where it caches no result, or where it reads what no position names: a
/// name, a volatile function, a reference over several sheets. A structural
/// edit makes every formula stale.
SheetRecalculation recalculate(const abstract::Document &document);

/// Whether an edit since the last recalculation left a formula stale.
[[nodiscard]] bool is_edited(const abstract::Document &document);

} // namespace odr::internal
