#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace odr::internal {

class TablePosition final {
public:
  static std::uint32_t to_column_num(const std::string &string);
  static std::uint32_t to_row_num(const std::string &string);
  static std::string to_column_string(std::uint32_t column);
  static std::string to_row_string(std::uint32_t row);

  TablePosition() noexcept;
  TablePosition(std::uint32_t column, std::uint32_t row) noexcept;
  explicit TablePosition(const std::string &);

  bool operator==(const TablePosition &rhs) const;
  bool operator!=(const TablePosition &rhs) const;

  [[nodiscard]] std::uint32_t column() const noexcept;
  [[nodiscard]] std::uint32_t row() const noexcept;
  [[nodiscard]] std::string to_string() const noexcept;
  [[nodiscard]] std::size_t hash() const noexcept;

private:
  std::uint32_t m_column{0};
  std::uint32_t m_row{0};
};

} // namespace odr::internal

template <> struct std::hash<odr::internal::TablePosition> {
  std::size_t operator()(const odr::internal::TablePosition &k) const noexcept;
};
