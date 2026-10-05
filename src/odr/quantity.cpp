#include <odr/quantity.hpp>

#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace odr {

double QuantityBase::parse_magnitude(std::string_view &text) {
  internal::TextCursor cursor(text);
  cursor.skip_whitespace();
  const std::string_view start = cursor.rest();
  if (cursor.peek() == '+' || cursor.peek() == '-') {
    cursor.advance(1);
  }
  const auto digit = internal::util::string::is_ascii_digit;
  (void)cursor.take_while(digit);
  if (cursor.peek() == '.') {
    cursor.advance(1);
    (void)cursor.take_while(digit);
  }
  if (cursor.peek() == 'e' || cursor.peek() == 'E') {
    const std::string_view exponent = cursor.rest();
    cursor.advance(1);
    if (cursor.peek() == '+' || cursor.peek() == '-') {
      cursor.advance(1);
    }
    if (cursor.take_while(digit).empty()) {
      cursor.seek(exponent); // An `em` or `ex` unit is not an exponent.
    }
  }
  const auto magnitude = internal::util::number::parse(
      start.substr(0, start.size() - cursor.rest().size()));
  if (!magnitude) {
    throw std::invalid_argument("invalid quantity magnitude");
  }
  cursor.skip_whitespace();
  text = cursor.rest();
  return *magnitude;
}

/// 7 digits: enough for drawing coordinates in the thousands of mm, and no more
/// than a `float` carries, so `68.55` does not come back as `68.550003`.
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
  /// Transparent, so the lookup path does not have to allocate a key.
  struct Hash final {
    using is_transparent = void;

    std::size_t operator()(const std::string_view name) const noexcept {
      return std::hash<std::string_view>{}(name);
    }
  };

  static Registry &registry_() {
    static Registry registry;
    return registry;
  }

  Registry() = default;

  std::shared_mutex m_mutex;
  std::unordered_map<std::string, std::unique_ptr<Unit>, Hash, std::equal_to<>>
      m_registry;

  /// Unit addresses remain stable; concurrent lookups share the read lock.
  const Unit *unit_(const std::string_view name) {
    {
      const std::shared_lock lock(m_mutex);
      if (const auto it = m_registry.find(name); it != m_registry.end()) {
        return it->second.get();
      }
    }

    const std::unique_lock lock(m_mutex);
    std::unique_ptr<Unit> &unit = m_registry[std::string(name)];
    if (unit == nullptr) {
      unit = std::make_unique<Unit>();
      unit->name = name;
    }
    return unit.get();
  }
};

/// Registered, not null, so that it equals the unit `Measure("5")` parses.
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
