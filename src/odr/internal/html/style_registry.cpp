#include <odr/internal/html/style_registry.hpp>

#include <ostream>

namespace odr::internal::html {

namespace {

/// Each name is written once per element carrying the declaration, so a
/// character costs more than it looks.
std::string name(std::string_view prefix, std::size_t index,
                 const std::size_t base) {
  static constexpr std::string_view digits =
      "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string suffix;
  do {
    suffix.insert(suffix.begin(), digits[index % base]);
    index /= base;
  } while (index != 0);
  return std::string(prefix) + suffix;
}

const std::string unnamed;

} // namespace

const std::string &StyleRegistry::intern(const std::string_view prefix,
                                         std::string declaration) {
  if (declaration.empty()) {
    return unnamed;
  }

  if (m_closed) {
    const auto it = m_entries.find(declaration);
    return it == m_entries.end() ? unnamed : it->second;
  }

  const auto [it, inserted] = m_entries.try_emplace(std::move(declaration));
  if (inserted) {
    it->second = name(prefix, m_count_by_prefix[std::string(prefix)]++,
                      m_digits == Digits::base36 ? 36 : 10);
    m_order.push_back(&*it);
  }
  return it->second;
}

void StyleRegistry::write_rules(std::ostream &out) const {
  for (const auto *entry : m_order) {
    const std::string &name = entry->second;
    out << "\n." << name;
    if (m_rank == Rank::replaces_inline) {
      // The specificity the inline `style` it stands in for had. Still under
      // `!important`, which the dark sheet needs.
      out << '.' << name << '.' << name;
    }
    const std::string_view block = entry->first;
    out << '{'
        << (block.back() == ';' ? block.substr(0, block.size() - 1) : block)
        << '}';
  }
}

} // namespace odr::internal::html
