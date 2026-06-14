#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace odr::internal::pdf {

/// Maps PDF character codes to Unicode, as described by a `ToUnicode` CMap.
///
/// A code is a sequence of bytes (1 or more, big-endian) whose width is defined
/// by the codespace ranges; a destination is a sequence of UTF-16 units (more
/// than one for ligatures). `translate_string` splits an input string into
/// codes of the right width and concatenates their destinations.
class CMap {
public:
  void add_codespace_range(std::string low_code, std::string high_code);
  void map_single(std::string code, std::u16string unicode);

  /// True when no code -> Unicode mapping was parsed (e.g. the font carries no
  /// `ToUnicode` CMap); the caller then falls back to the `/Encoding`.
  [[nodiscard]] bool empty() const { return m_map.empty(); }

  [[nodiscard]] std::string translate_string(const std::string &codes) const;

private:
  struct CodespaceRange {
    // `low` and `high` share the same length; that length is the code width in
    // bytes for codes that start within this range.
    std::string low;
    std::string high;
  };

  std::vector<CodespaceRange> m_codespace_ranges;
  std::unordered_map<std::string, std::u16string> m_map;

  /// Byte width of the code starting at `pos`, decided by the codespace ranges;
  /// falls back to a single byte when no range declares/matches it.
  [[nodiscard]] std::size_t code_length(const std::string &codes,
                                        std::size_t pos) const;
};

} // namespace odr::internal::pdf
