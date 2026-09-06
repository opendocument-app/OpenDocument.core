#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal::html {

/// Names each distinct style block a class, defined once in `<head>`. An
/// inline `style` is the one shape a browser cannot share across the cells of
/// a sheet.
class StyleRegistry {
public:
  /// The class @p style is written as; `nullptr` leaves it inline.
  const std::string *use(const std::string &style);

  /// Names nothing further, so a block first seen after this stays inline.
  void close() { m_closed = true; }

  [[nodiscard]] bool has_rules() const { return !m_order.empty(); }

  /// One rule per line, each preceded by a newline.
  void write_rules(std::ostream &out) const;

private:
  /// Node-based: the pointers in `m_order` outlive every insertion.
  std::unordered_map<std::string, std::string> m_entries;
  std::vector<const std::pair<const std::string, std::string> *> m_order;
  bool m_closed{false};
};

} // namespace odr::internal::html
