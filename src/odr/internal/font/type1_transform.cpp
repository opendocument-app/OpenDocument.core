#include <odr/internal/font/type1_transform.hpp>

#include <odr/internal/font/cff_builder.hpp>
#include <odr/internal/font/type1_charstring.hpp>
#include <odr/internal/font/type1_font.hpp>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace odr::internal::font {

std::string type1::to_cff(const Type1Font &font) {
  // Order glyphs with `.notdef` at index 0 (CFF requires it). Translate each
  // Type1 charstring to Type2; the width rides in the charstring (the CFF
  // builder uses nominalWidthX = 0).
  std::vector<cff::BuilderGlyph> glyphs;
  glyphs.reserve(font.glyphs().size() + 1);

  // CFF glyph names must be unique; a repeated Type1 name keeps its first
  // charstring.
  std::unordered_set<std::string_view> names;
  const auto translate = [&](const Glyph &glyph) {
    if (!names.insert(glyph.name).second) {
      return;
    }
    // A charstring that does not translate becomes an empty glyph, so one
    // broken glyph does not refuse the font.
    std::string charstring;
    try {
      charstring = to_type2(glyph.charstring, font.subrs());
    } catch (const std::runtime_error &) {
      charstring.assign(1, static_cast<char>(14));
    }
    glyphs.push_back({glyph.name, std::move(charstring)});
  };

  // .notdef first.
  std::size_t notdef = font.glyphs().size();
  for (std::size_t i = 0; i < font.glyphs().size(); ++i) {
    if (font.glyphs()[i].name == ".notdef") {
      notdef = i;
      break;
    }
  }
  if (notdef < font.glyphs().size()) {
    translate(font.glyphs()[notdef]);
  } else {
    names.insert(".notdef");
    glyphs.push_back({".notdef", std::string(1, static_cast<char>(14))});
  }
  for (std::size_t i = 0; i < font.glyphs().size(); ++i) {
    if (i != notdef) {
      translate(font.glyphs()[i]);
    }
  }

  return cff::build_cff(font.name(), glyphs, /*default_width=*/0,
                        /*nominal_width=*/0, font.font_bbox());
}

} // namespace odr::internal::font
