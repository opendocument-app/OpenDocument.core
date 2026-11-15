#pragma once

#include <odr/definitions.hpp>
#include <odr/table_position.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

class SheetDependencies;

/// What a recalculation reads off a workbook's edits: the dependency graph,
/// and the cells written since the last recalculation.
class SheetEdits final {
public:
  explicit SheetEdits(const abstract::Document *document);
  SheetEdits(const SheetEdits &) = delete;
  SheetEdits(SheetEdits &&) = delete;
  SheetEdits &operator=(const SheetEdits &) = delete;
  SheetEdits &operator=(SheetEdits &&) = delete;
  ~SheetEdits();

  /// Built on the first question and kept until formulas change.
  [[nodiscard]] const SheetDependencies &dependencies();

  /// Marks dependents of (@p sheet, @p position) for recalculation.
  void note_written(ElementIdentifier sheet, const TablePosition &position);
  /// Notes a write that adds or removes a formula.
  void note_formula_changed() noexcept;
  /// Notes an edit that moves cells: a recalculation computes every formula
  /// again.
  void note_moved() noexcept;

  /// The writes noted since the last recalculation, by sheet.
  [[nodiscard]] const std::vector<std::pair<ElementIdentifier, TablePosition>> &
  written() const noexcept;
  [[nodiscard]] bool moved() const noexcept;
  /// Clears the edits handled by recalculation.
  void forget() noexcept;

private:
  const abstract::Document *m_document{nullptr};
  std::unique_ptr<SheetDependencies> m_dependencies;
  std::vector<std::pair<ElementIdentifier, TablePosition>> m_written;
  bool m_moved{false};
};

} // namespace odr::internal
