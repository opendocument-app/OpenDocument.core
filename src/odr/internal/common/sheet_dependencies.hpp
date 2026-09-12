#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

/// Which cells a formula reads, as the reverse map an edit asks: the cells
/// that go stale when a position changes. Built once off a decoded document.
class SheetDependencies final {
public:
  SheetDependencies() = default;

  /// Walks every sheet of @p document and parses every formula it states.
  /// Empty where the engine states none (`.xls`, `.numbers`, a text file).
  [[nodiscard]] static SheetDependencies of(const abstract::Document &document);

  /// The cells whose formula reads one of @p positions, directly or through
  /// another formula. Sorted, and each named once.
  [[nodiscard]] std::vector<SheetPosition>
  dependents(const std::vector<SheetPosition> &positions) const;

  /// The cells holding a formula whose references could not all be read — one
  /// that does not parse, names a name, or reaches over several sheets.
  [[nodiscard]] const std::vector<SheetPosition> &unresolved() const noexcept;

private:
  /// The rectangle of one sheet a formula reads.
  struct Read final {
    std::uint32_t sheet{0};
    TableRange range{};

    [[nodiscard]] bool contains(const SheetPosition &position) const;
  };

  struct Entry final {
    SheetPosition cell;
    std::vector<Read> reads;
  };

  /// Reads @p expression and records what it names, resolving a sheet name
  /// through @p by_name.
  void
  add_formula_(const SheetPosition &cell, const std::string &expression,
               formula::Syntax syntax,
               const std::unordered_map<std::string, std::uint32_t> &by_name);

  std::vector<Entry> m_entries;
  /// The entries reading a sheet, by that sheet — so a position is asked of
  /// the formulas that could name it rather than of all of them.
  std::unordered_map<std::uint32_t, std::vector<std::size_t>> m_by_sheet;
  std::vector<SheetPosition> m_unresolved;
};

} // namespace odr::internal
