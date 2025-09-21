#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace odr {

/// @brief Represents a runtime unit of measure.
class DynamicUnit {
public:
  DynamicUnit();
  explicit DynamicUnit(const char *name);

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

/// @brief Represents a quantity with a magnitude and a unit of measure.
template <typename Magnitude, typename Unit = DynamicUnit> class Quantity {
public:
  Quantity(Magnitude magnitude, Unit unit)
      : m_magnitude{std::move(magnitude)}, m_unit{std::move(unit)} {}

  explicit Quantity(const char *string) {
    char *end{nullptr};
    m_magnitude = std::strtod(string, &end);
    while (*end && std::isspace(*end)) {
      ++end;
    }
    m_unit = DynamicUnit(end);
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

  void to_stream(std::ostream &out) const {
    out << m_magnitude << m_unit.to_string();
  }

  [[nodiscard]] std::string to_string() const {
    std::ostringstream ss;
    ss << std::setprecision(4) << m_magnitude << m_unit.to_string();
    return ss.str();
  }

private:
  Magnitude m_magnitude{0};
  Unit m_unit;
};

} // namespace odr

std::ostream &operator<<(std::ostream &, const odr::DynamicUnit &);

template <typename Magnitude, typename Unit>
std::ostream &operator<<(std::ostream &out,
                         const odr::Quantity<Magnitude, Unit> &quantity) {
  quantity.to_stream(out);
  return out;
}
