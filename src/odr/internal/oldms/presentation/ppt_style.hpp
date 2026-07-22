#pragma once

#include <odr/style.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::presentation {

/// One character-formatting run of a StyleTextPropAtom ([MS-PPT] 2.9.46);
/// each field is set only when its CFMasks bit is.
struct TextCFRun final {
  std::uint32_t count{0}; //< characters this run covers
  std::optional<bool> bold;
  std::optional<bool> italic;
  std::optional<bool> underline;
  std::optional<std::uint16_t> font_ref; //< index into the FontCollection
  std::optional<std::int16_t> font_size; //< points
  std::optional<Color> color; //< explicit sRGB only; scheme indexes are unset
};

/// Parses a StyleTextPropAtom body ([MS-PPT] 2.9.44): the paragraph-level runs
/// are skipped, the character-level runs returned. `char_count` is the
/// corresponding text's length in characters plus one (the runs also cover an
/// implicit final paragraph mark); fewer covered characters end the parse.
std::vector<TextCFRun> parse_style_text_prop_atom(std::string_view body,
                                                  std::size_t char_count);

/// What character-formatting resolution needs: the document's font names
/// (interned) and the style of unformatted text.
struct StyleContext final {
  std::vector<const char *> fonts;
  TextStyle default_style;
};

/// The style of unformatted text: 18pt, approximating the unread master text
/// styles ([MS-PPT] 2.9.36 TextMasterStyleAtom).
[[nodiscard]] TextStyle default_character_style();

/// A character run's formatting on top of the default style. Throws on a font
/// reference outside `context.fonts`.
TextStyle resolve_style(const TextCFRun &run, const StyleContext &context);

} // namespace odr::internal::oldms::presentation
