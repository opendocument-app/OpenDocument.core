#pragma once

#include <odr/document_element.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::iwork {
class Budget;
class Package;

/// A `TST.TableModelArchive`: its name, the extent it declares, and the cells
/// that hold something.
struct TableModel final {
  /// One cell a tile carries. A position the tile does not carry is empty and
  /// has no entry here.
  struct Cell final {
    std::uint32_t row{};
    std::uint32_t column{};
    ValueType value_type{ValueType::unknown};
    /// The value as text. Empty for a rich text cell, whose paragraphs live in
    /// the storage @ref storage_identifier names.
    std::string text;
    std::optional<std::uint64_t> storage_identifier;
  };

  std::string name;
  std::uint32_t rows{};
  std::uint32_t columns{};
  std::vector<Cell> cells;
};

/// Reads a table within @p budget; unknown drawable kinds yield an empty model.
TableModel read_table(Package &package, Budget &budget,
                      std::uint64_t identifier);

/// Caches decoded table models by identifier for one parse.
class TableCache final {
public:
  const TableModel &table(Package &package, Budget &budget,
                          std::uint64_t identifier);

private:
  std::unordered_map<std::uint64_t, TableModel> m_tables;
};

/// An IEEE 754 decimal128, in the binary integer form Apple writes, as an
/// exact decimal string. Exposed for its tests.
std::string decimal128_to_string(std::string_view bytes);

/// @p seconds since 2001-01-01T00:00:00Z as an ISO 8601 instant. Exposed for
/// its tests.
std::string date_to_string(double seconds);

/// @p seconds as the `1d 2h 3m 4s` form Numbers shows a duration in. Exposed
/// for its tests.
std::string duration_to_string(double seconds);

} // namespace odr::internal::iwork
