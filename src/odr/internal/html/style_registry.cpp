#include <odr/internal/html/style_registry.hpp>

#include <ostream>
#include <string_view>

namespace odr::internal::html {

namespace {

/// Base 36, so the first 36 blocks fit in two characters. Each name is written
/// once per element carrying the block.
std::string name(std::size_t index) {
  static constexpr std::string_view digits =
      "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string suffix;
  do {
    suffix.insert(suffix.begin(), digits[index % digits.size()]);
    index /= digits.size();
  } while (index != 0);
  return 'c' + suffix;
}

} // namespace

const std::string *StyleRegistry::use(const std::string &style) {
  if (style.empty()) {
    return nullptr;
  }

  if (m_closed) {
    const auto it = m_entries.find(style);
    return it == m_entries.end() ? nullptr : &it->second;
  }

  const auto [it, inserted] = m_entries.try_emplace(style);
  if (inserted) {
    it->second = name(m_order.size());
    m_order.push_back(&*it);
  }
  return &it->second;
}

void StyleRegistry::write_rules(std::ostream &out) const {
  for (const auto *entry : m_order) {
    // Named three times for the specificity an inline `style` had. Still under
    // `!important`, which the dark sheet needs.
    const std::string &name = entry->second;
    out << "\n." << name << '.' << name << '.' << name << '{';
    const std::string_view block = entry->first;
    out << (block.back() == ';' ? block.substr(0, block.size() - 1) : block);
    out << '}';
  }
}

} // namespace odr::internal::html
