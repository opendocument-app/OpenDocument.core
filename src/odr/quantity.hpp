#pragma once

#include <cmath>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace odr {

/// Represents a runtime unit of measure.
class DynamicUnit {
public:
  /// Constructs the unitless unit, equal to `DynamicUnit("")`.
  DynamicUnit();
  explicit DynamicUnit(std::string_view name);

  bool operator==(const DynamicUnit &rhs) const;
  bool operator!=(const DynamicUnit &rhs) const;

  [[nodiscard]] const std::string &name() const;

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  struct Unit;
  class Registry;

  const Unit *m_unit{nullptr};
};

/// The magnitude-type-independent part of @ref Quantity, so it can be compiled
/// once in a source file instead of inline in every instantiation.
class QuantityBase {
protected:
  static double parse_magnitude(std::string_view &text);

  /// Renders @p magnitude with 7 significant digits, always positional.
  static std::string format_magnitude(double magnitude);
};

/// Represents a quantity with a magnitude and a unit of measure.
template <typename Magnitude, typename Unit = DynamicUnit>
class Quantity : private QuantityBase {
public:
  Quantity(Magnitude magnitude, Unit unit)
      : m_magnitude{std::move(magnitude)}, m_unit{std::move(unit)} {}

  explicit Quantity(const std::string_view string) {
    std::string_view rest = string;
    const double magnitude = parse_magnitude(rest);
    if constexpr (std::is_integral_v<Magnitude>) {
      const double limit =
          std::ldexp(1.0, std::numeric_limits<Magnitude>::digits);
      const double minimum = std::is_signed_v<Magnitude> ? -limit : 0;
      if (magnitude < minimum || magnitude >= limit) {
        throw std::out_of_range("quantity magnitude out of range");
      }
    }
    m_magnitude = static_cast<Magnitude>(magnitude);
    m_unit = Unit(rest);
  }

  bool operator==(const Quantity &rhs) const {
    return m_magnitude == rhs.m_magnitude && m_unit == rhs.m_unit;
  }
  bool operator!=(const Quantity &rhs) const {
    return m_magnitude != rhs.m_magnitude || m_unit != rhs.m_unit;
  }

  explicit operator Magnitude() const { return m_magnitude; }

  Magnitude magnitude() const { return m_magnitude; }
  Unit unit() const { return m_unit; }

  void to_stream(std::ostream &out) const { out << to_string(); }

  [[nodiscard]] std::string to_string() const {
    if constexpr (std::is_floating_point_v<Magnitude>) {
      return format_magnitude(static_cast<double>(m_magnitude)) +
             m_unit.to_string();
    } else {
      return std::to_string(m_magnitude) + m_unit.to_string();
    }
  }

private:
  Magnitude m_magnitude{0};
  Unit m_unit;
};

/// Represents a quantity: a magnitude and a unit of measure.
using Measure = Quantity<double>;

} // namespace odr

std::ostream &operator<<(std::ostream &, const odr::DynamicUnit &);

template <typename Magnitude, typename Unit>
std::ostream &operator<<(std::ostream &out,
                         const odr::Quantity<Magnitude, Unit> &quantity) {
  quantity.to_stream(out);
  return out;
}
