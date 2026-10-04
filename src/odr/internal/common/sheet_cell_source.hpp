#pragma once

#include <odr/document_element.hpp>
#include <odr/sheet_position.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

/// The cells of a decoded document, as the evaluator reads them. A formula
/// cell gives the result the file caches, and no answer where it caches none.
class SheetCellSource final : public formula::CellSource {
public:
  explicit SheetCellSource(const abstract::Document &document);

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(std::string_view name) const override;
  [[nodiscard]] std::optional<formula::Value>
  cell(const SheetPosition &position) const override;
  [[nodiscard]] TableDimensions extent(std::uint32_t sheet) const override;

  /// The settings the document states, which a cell's date is read with.
  [[nodiscard]] const formula::Settings &settings() const noexcept;

private:
  std::vector<Sheet> m_sheets;
  /// The sheets by their name in lower case.
  std::unordered_map<std::string, std::uint32_t> m_by_name;
  formula::Settings m_settings;
  mutable std::unordered_map<SheetPosition, std::optional<formula::Value>>
      m_cells;
  mutable std::unordered_map<std::uint32_t, TableDimensions> m_extents;
};

} // namespace odr::internal
