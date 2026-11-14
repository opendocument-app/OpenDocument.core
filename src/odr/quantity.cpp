#include <odr/quantity.hpp>

#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <shared_mutex>
#include <tuple>
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
  if (!magnitude.has_value()) {
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
  KnownUnit known{KnownUnit::unknown};
  Dimension dimension{Dimension::unknown};
  /// The unit's size in the SI unit of its dimension (metre, radian, 1) as a
  /// ratio, so that units both stated in whole numbers convert exactly.
  double numerator{0};
  double denominator{1};
};

namespace {

using KnownUnitRow =
    std::tuple<std::string_view, KnownUnit, Dimension, double, double>;

constexpr double pi = std::numbers::pi;

/// In `KnownUnit` order.
constexpr std::array<KnownUnitRow, 14> known_units{{
    {"", KnownUnit::none, Dimension::dimensionless, 1, 1},
    {"%", KnownUnit::percent, Dimension::dimensionless, 1, 100},
    {"m", KnownUnit::metre, Dimension::length, 1, 1},
    {"cm", KnownUnit::centimetre, Dimension::length, 1, 100},
    {"mm", KnownUnit::millimetre, Dimension::length, 1, 1000},
    {"q", KnownUnit::quarter_millimetre, Dimension::length, 1, 4000},
    {"in", KnownUnit::inch, Dimension::length, 127, 5000},
    {"pt", KnownUnit::point, Dimension::length, 127, 360000},
    {"pc", KnownUnit::pica, Dimension::length, 127, 30000},
    {"px", KnownUnit::pixel, Dimension::length, 127, 480000},
    {"rad", KnownUnit::radian, Dimension::angle, 1, 1},
    {"deg", KnownUnit::degree, Dimension::angle, pi, 180},
    {"grad", KnownUnit::gradian, Dimension::angle, pi, 200},
    {"turn", KnownUnit::turn, Dimension::angle, 2 * pi, 1},
}};

} // namespace

class DynamicUnit::Registry final {
public:
  static const Unit *unit(const std::string_view name) {
    return registry_().unit_(name);
  }

  static const Unit *unit(const KnownUnit known) {
    const auto index = static_cast<std::size_t>(known);
    if (index == 0 || index > known_units.size()) {
      throw std::invalid_argument("not a known unit");
    }
    return registry_().m_known[index - 1];
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

  Registry() {
    for (std::size_t i = 0; i < known_units.size(); ++i) {
      const auto &[name, known, dimension, numerator, denominator] =
          known_units[i];
      std::unique_ptr<Unit> &unit = m_registry[std::string(name)];
      unit = std::make_unique<Unit>(Unit{.name = std::string(name),
                                         .known = known,
                                         .dimension = dimension,
                                         .numerator = numerator,
                                         .denominator = denominator});
      m_known[i] = unit.get();
    }
  }

  std::shared_mutex m_mutex;
  std::unordered_map<std::string, std::unique_ptr<Unit>, Hash, std::equal_to<>>
      m_registry;
  /// Written by the constructor only, so read without the lock.
  std::array<const Unit *, known_units.size()> m_known{};

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

DynamicUnit::DynamicUnit(const KnownUnit unit) : m_unit{Registry::unit(unit)} {}

bool DynamicUnit::operator==(const DynamicUnit &rhs) const {
  return m_unit == rhs.m_unit;
}

bool DynamicUnit::operator!=(const DynamicUnit &rhs) const {
  return m_unit != rhs.m_unit;
}

const std::string &DynamicUnit::name() const { return m_unit->name; }

KnownUnit DynamicUnit::known() const { return m_unit->known; }

Dimension DynamicUnit::dimension() const { return m_unit->dimension; }

void DynamicUnit::to_stream(std::ostream &out) const { out << m_unit->name; }

std::string DynamicUnit::to_string() const { return m_unit->name; }

double QuantityBase::convert(const double magnitude, const DynamicUnit &from,
                             const DynamicUnit &to) {
  if (from == to) {
    return magnitude;
  }
  if (from.dimension() == Dimension::unknown ||
      from.dimension() != to.dimension()) {
    throw std::invalid_argument("cannot convert " + from.name() + " to " +
                                to.name());
  }
  return magnitude * ((from.m_unit->numerator * to.m_unit->denominator) /
                      (from.m_unit->denominator * to.m_unit->numerator));
}

} // namespace odr

std::ostream &operator<<(std::ostream &out, const odr::DynamicUnit &unit) {
  unit.to_stream(out);
  return out;
}
