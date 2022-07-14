#include <memory>
#include <odr/quantity.hpp>
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

  std::unordered_map<std::string, std::unique_ptr<DynamicUnit::Unit>>
      m_registry;

  const Unit *unit_(const char *name) {
    auto &&unit = m_registry[name];
    if (!unit) {
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

std::string DynamicUnit::to_string() const { return m_unit->name; }

} // namespace odr
