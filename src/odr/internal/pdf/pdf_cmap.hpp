#pragma once

#include <string>
#include <unordered_map>

namespace odr::internal::pdf {

class CMap {
public:
  void map_bfchar(char glyph, char16_t unicode);

  [[nodiscard]] char16_t translate_glyph(char glyph) const;
  [[nodiscard]] std::string translate_string(const std::string &glyphs) const;

private:
  std::unordered_map<char, char16_t> m_bfchar;
};

} // namespace odr::internal::pdf
