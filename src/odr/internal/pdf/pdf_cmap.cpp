#include <odr/internal/pdf/pdf_cmap.hpp>

#include <odr/internal/util/map_util.hpp>

#include <utf8cpp/utf8/cpp17.h>

namespace odr::internal::pdf {

CMap::CMap() = default;

void CMap::map_bfchar(char glyph, char16_t unicode) {
  m_bfchar[glyph] = unicode;
}

char16_t CMap::translate_glyph(char glyph) const {
  return util::map::lookup_default(m_bfchar, glyph, glyph);
}

std::string CMap::translate_string(const std::string &glyphs) const {
  std::u16string result;

  for (char glyph : glyphs) {
    result += translate_glyph(glyph);
  }

  return utf8::utf16to8(result);
}

} // namespace odr::internal::pdf
