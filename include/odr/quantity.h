#ifndef ODR_QUANTITY_H
#define ODR_QUANTITY_H

#include <string>

namespace odr {

class DynamicUnit {
public:
  DynamicUnit();
  explicit DynamicUnit(const char *name);

  bool operator==(const DynamicUnit &rhs) const;
  bool operator!=(const DynamicUnit &rhs) const;

  [[nodiscard]] const std::string &name() const;

private:
  struct Unit;
  class Registry;

  const Unit *m_unit{nullptr};
};

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

  explicit operator Magnitude() const { return m_magnitude; }

  Magnitude magnitude() const { return m_magnitude; }
  Unit unit() const { return m_unit; }

private:
  Magnitude m_magnitude{0};
  Unit m_unit;
};

} // namespace odr

#endif // ODR_QUANTITY_H
