#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::html {

/// Names each distinct declaration a class, written once in `<head>`. The same
/// declarations recur across the cells of a sheet and the positioned elements
/// of a pdf page, and an inline `style` is the one shape a browser cannot
/// share: each is parsed into its own block, and each element gets its own
/// computed style.
class StyleRegistry {
public:
  /// What a rule has to outrank.
  enum class Rank {
    plain,           ///< nothing else styles the elements carrying it
    replaces_inline, ///< the class three times, for the specificity it took
                     ///< over
  };

  /// How the number after a prefix is written. `base36` names the first 36 of a
  /// prefix in one character, but only a caller whose prefixes never extend one
  /// another may ask for it: `w` at 1008 spells `ws0`, which is also `ws` at 0.
  enum class Digits { decimal, base36 };

  explicit StyleRegistry(const Rank rank, const Digits digits = Digits::decimal)
      : m_rank{rank}, m_digits{digits} {}

  /// The class @p declaration is written as, `<prefix><n>`. Empty where it
  /// cannot be named: an empty declaration, or one first seen after `close`.
  const std::string &intern(std::string_view prefix, std::string declaration);

  /// Names nothing further, so a declaration first seen after this stays
  /// inline.
  void close() { m_closed = true; }

  [[nodiscard]] bool has_rules() const { return !m_order.empty(); }

  /// One rule per line, each preceded by a newline.
  void write_rules(std::ostream &out) const;

private:
  /// Node-based: the pointers in `m_order` outlive every insertion.
  std::unordered_map<std::string, std::string> m_entries;
  std::vector<const std::pair<const std::string, std::string> *> m_order;
  std::unordered_map<std::string, std::size_t> m_count_by_prefix;
  Rank m_rank{Rank::plain};
  Digits m_digits{Digits::decimal};
  bool m_closed{false};
};

} // namespace odr::internal::html
