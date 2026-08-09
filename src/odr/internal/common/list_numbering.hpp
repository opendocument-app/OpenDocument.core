#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal {

enum class ListNumberFormat {
  none,
  bullet,
  decimal,
  decimal_zero,
  letter_lower,
  letter_upper,
  roman_lower,
  roman_upper,
};

/// @brief One level of a list style, in the shape ODF and OOXML both lower to.
///
/// `label` is OOXML's `w:lvlText`: literal text in which `%N` stands for the
/// counter of level N, counted from 1. ODF has no such template — its
/// prefix / suffix / `text:display-levels` compose into one.
struct ListLevel final {
  ListNumberFormat format{ListNumberFormat::none};
  std::string label;
  std::uint32_t start{1};
};

/// @brief What one list item is labelled with, once resolved.
struct ListMarker final {
  std::string text;
  std::optional<std::uint32_t> number;
};

std::string format_list_number(ListNumberFormat format, std::uint32_t number);

/// @brief The counters of one list, indexed by level.
class ListCounter final {
public:
  /// Advances `level` (counted from 0), resets the levels below it, and expands
  /// that level's label against the resulting counters.
  std::string advance(std::uint32_t level, const ListLevel &list_level);

  /// The counter `advance` last produced for `level`.
  [[nodiscard]] std::uint32_t number(std::uint32_t level) const;

  void restart(std::uint32_t level, std::uint32_t number);

private:
  std::vector<std::uint32_t> m_numbers;
  std::vector<ListNumberFormat> m_formats;

  void grow_(std::uint32_t level);
};

} // namespace odr::internal
