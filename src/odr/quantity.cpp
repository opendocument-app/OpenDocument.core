#include <odr/quantity.hpp>

#include <memory>
#include <unordered_map>

namespace odr {

struct DynamicUnit::Unit final {
  std::string name;
};

class DynamicUnit::Registry final {
public:
  static const Unit *unit(const char *name) { return registry_().unit_(name); }

private:
  static Registry &registry_() {
    static Registry registry;
    return registry;
  }

  Registry() = default;

  std::unordered_map<std::string, std::unique_ptr<Unit>> m_registry;

  const Unit *unit_(const char *name) {
    std::unique_ptr<Unit> &unit = m_registry[name];
    if (unit == nullptr) {
      unit = std::make_unique<Unit>();
      unit->name = name;
    }
    return unit.get();
  }
};

DynamicUnit::DynamicUnit() = default;

DynamicUnit::DynamicUnit(const char *name) : m_unit{Registry::unit(name)} {}

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
