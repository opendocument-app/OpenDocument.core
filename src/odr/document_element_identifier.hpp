#pragma once

#include <compare>
#include <cstdint>
#include <utility>

namespace odr {

using ElementIdentifier = std::uint64_t;

static constexpr ElementIdentifier null_element_id{0};

class ExtendedElementIdentifier {
public:
  using Extra = std::uint32_t;

  static constexpr Extra no_extra{0};

  static constexpr ExtendedElementIdentifier null() noexcept { return {}; }
  static constexpr ExtendedElementIdentifier
  make_column(const ElementIdentifier element_id,
              const std::uint32_t column) noexcept {
    return ExtendedElementIdentifier{element_id, column + 1, no_extra};
  }
  static constexpr ExtendedElementIdentifier
  make_row(const ElementIdentifier element_id,
           const std::uint32_t row) noexcept {
    return ExtendedElementIdentifier{element_id, no_extra, row + 1};
  }
  static constexpr ExtendedElementIdentifier
  make_cell(const ElementIdentifier element_id, const std::uint32_t column,
            const std::uint32_t row) noexcept {
    return ExtendedElementIdentifier{element_id, column + 1, row + 1};
  }

  constexpr ExtendedElementIdentifier() noexcept = default;

  explicit constexpr ExtendedElementIdentifier(
      const ElementIdentifier element_id, const Extra extra0 = no_extra,
      const Extra extra1 = no_extra) noexcept
      : m_element_id{element_id}, m_extra0{extra0}, m_extra1{extra1} {}

  [[nodiscard]] constexpr ElementIdentifier element_id() const noexcept {
    return m_element_id;
  }
  [[nodiscard]] constexpr Extra extra0() const noexcept { return m_extra0; }
  [[nodiscard]] constexpr Extra extra1() const noexcept { return m_extra1; }

  [[nodiscard]] constexpr bool is_null() const noexcept {
    return m_element_id == null_element_id;
  }
  [[nodiscard]] constexpr bool has_extra0() const noexcept {
    return m_extra0 != no_extra;
  }
  [[nodiscard]] constexpr bool has_extra1() const noexcept {
    return m_extra1 != no_extra;
  }

  [[nodiscard]] constexpr bool has_column() const noexcept {
    return has_extra0() && !has_extra1();
  }
  [[nodiscard]] constexpr bool has_row() const noexcept {
    return !has_extra0() && has_extra1();
  }
  [[nodiscard]] constexpr bool has_cell() const noexcept {
    return has_extra0() && has_extra1();
  }

  [[nodiscard]] std::uint32_t column() const noexcept { return m_extra0 - 1; }
  [[nodiscard]] std::uint32_t row() const noexcept { return m_extra1 - 1; }
  [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> cell() const noexcept {
    return {column(), row()};
  }

private:
  ElementIdentifier m_element_id{null_element_id};
  Extra m_extra0{no_extra};
  Extra m_extra1{no_extra};

  friend constexpr bool
  operator==(const ExtendedElementIdentifier &lhs,
             const ExtendedElementIdentifier &rhs) noexcept {
    return lhs.m_element_id == rhs.m_element_id &&
           lhs.m_extra0 == rhs.m_extra0 && lhs.m_extra1 == rhs.m_extra1;
  }

  friend constexpr std::strong_ordering
  operator<=>(const ExtendedElementIdentifier &a,
              const ExtendedElementIdentifier &b) noexcept {
    if (const std::strong_ordering cmp = a.m_element_id <=> b.m_element_id;
        cmp != 0) {
      return cmp;
    }
    if (const std::strong_ordering cmp = a.m_extra0 <=> b.m_extra0; cmp != 0) {
      return cmp;
    }
    return a.m_extra1 <=> b.m_extra1;
  }
};

} // namespace odr
