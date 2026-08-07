#include <odr/quantity.hpp>

#include <odr/internal/util/number_util.hpp>

#include <memory>
#include <unordered_map>

namespace odr {

/// 7 significant digits: one more than the stream default, which rounds
/// drawing coordinates in the thousands of mm, and no more than a `float`
/// carries, so `68.55` does not come back as `68.550003`.
std::string QuantityBase::format_magnitude(const double magnitude) {
  return internal::util::number::to_string_significant(magnitude, 7);
}

struct DynamicUnit::Unit final {
  std::string name;
};

class DynamicUnit::Registry final {
public:
  static const Unit *unit(const std::string_view name) {
    return registry_().unit_(name);
  }

private:
  static Registry &registry_() {
    static Registry registry;
    return registry;
  }

  Registry() = default;

  std::unordered_map<std::string, std::unique_ptr<Unit>> m_registry;

  const Unit *unit_(const std::string_view name) {
    std::unique_ptr<Unit> &unit = m_registry[std::string(name)];
    if (unit == nullptr) {
      unit = std::make_unique<Unit>();
      unit->name = name;
    }
    return unit.get();
  }
};

/// Registered rather than left null, so that `m_unit` is always dereferenceable
/// and this compares equal to the unit `Measure("5")` parses.
DynamicUnit::DynamicUnit() : m_unit{Registry::unit("")} {}

DynamicUnit::DynamicUnit(const std::string_view name)
    : m_unit{Registry::unit(name)} {}

bool DynamicUnit::operator==(const DynamicUnit &rhs) const {
  return m_unit == rhs.m_unit;
}

bool DynamicUnit::operator!=(const DynamicUnit &rhs) const {
  return m_unit != rhs.m_unit;
}

const std::string &DynamicUnit::name() const { return m_unit->name; }

void DynamicUnit::to_stream(std::ostream &out) const { out << m_unit->name; }

std::string DynamicUnit::to_string() const { return m_unit->name; }

} // namespace odr

std::ostream &operator<<(std::ostream &out, const odr::DynamicUnit &unit) {
  unit.to_stream(out);
  return out;
}
