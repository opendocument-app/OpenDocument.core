#pragma once

#include <odr/definitions.hpp>

#include <odr/internal/iwork/iwork_table.hpp>

#include <cstdint>
#include <stdexcept>

namespace odr::internal::iwork {
class Budget;
class ElementRegistry;
class Message;
class Package;

/// What a storage walk needs to resolve what its text anchors.
struct Context final {
  /// How deep a storage may nest before the file is treated as malformed: a
  /// table cell holds a storage, which may hold a table again.
  static constexpr std::uint32_t max_depth = 8;

  ElementRegistry *registry{nullptr};
  Package *package{nullptr};
  Budget *budget{nullptr};
  TableCache *tables{nullptr};
  std::uint32_t depth{};

  [[nodiscard]] Context deeper() const {
    if (depth + 1 >= max_depth) {
      throw std::runtime_error("iwork: storages nest too deeply");
    }
    return {registry, package, budget, tables, depth + 1};
  }
};

/// Appends storage paragraphs and their anchored drawables to @p parent_id.
/// Shared by Pages bodies, Keynote text boxes and rich-text cells.
void parse_storage(const Context &context, ElementIdentifier parent_id,
                   const Message &storage);

/// Fills @p cell_id with what the tile carried: a value as text, or the
/// storage a rich text cell holds.
void fill_cell(const Context &context, ElementIdentifier cell_id,
               const TableModel::Cell &cell);

} // namespace odr::internal::iwork
