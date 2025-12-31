#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace odr {

struct TablePosition final {
  static std::uint32_t to_column_num(const std::string &string);
  static std::uint32_t to_row_num(const std::string &string);
  static std::string to_column_string(std::uint32_t column);
  static std::string to_row_string(std::uint32_t row);

  std::uint32_t column{0};
  std::uint32_t row{0};

  constexpr TablePosition() noexcept = default;
  constexpr TablePosition(const std::uint32_t column_,
                          const std::uint32_t row_) noexcept
      : column{column_}, row{row_} {}
  explicit TablePosition(const std::string &string);

  bool operator==(const TablePosition &rhs) const;

  [[nodiscard]] std::string to_string() const noexcept;
  [[nodiscard]] std::size_t hash() const noexcept;
};

} // namespace odr

template <> struct std::hash<odr::TablePosition> {
  std::size_t operator()(const odr::TablePosition &k) const noexcept;
};
