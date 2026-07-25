#pragma once

#include <odr/style.hpp>

#include <cstdint>
#include <optional>
#include <string>
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

/// Owns the document's resolved character styles — indexed by the style index
/// stored on paragraph/span elements, 0 being the default style — and the
/// font names `TextStyle::font_name` points into. Immutable after
/// construction.
class StyleRegistry final {
public:
  StyleRegistry() = default;
  /// `font_names` are the FontCollection names the styles' `font_name` point
  /// into; `styles` are the resolved character styles, index 0 the default
  /// style.
  StyleRegistry(std::vector<std::string> font_names,
                std::vector<TextStyle> styles);

  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;

private:
  /// Owns the font names: `TextStyle::font_name` (`const char *`) points into
  /// them. Never modified after construction (moving the registry is fine —
  /// the strings themselves do not move).
  std::vector<std::string> m_font_names;
  std::vector<TextStyle> m_styles;
};

/// The style of unformatted text: 18pt, approximating the unread master text
/// styles ([MS-PPT] 2.9.36 TextMasterStyleAtom).
[[nodiscard]] TextStyle default_character_style();

/// Index of the default style (unformatted text) in a `StyleContext`'s
/// styles / the `StyleRegistry`.
constexpr std::uint32_t default_style_index = 0;

/// Accumulates the font names and resolved character styles while parsing;
/// moved into the `StyleRegistry` once the tree is built. `fonts` is indexed
/// by FontEntityAtom recInstance (an empty string marks a gap) and must be
/// complete before the first style is resolved — the styles' `font_name`
/// point into its strings.
struct StyleContext final {
  std::vector<std::string> fonts;
  std::vector<TextStyle> styles{default_character_style()};
};

/// Resolves a character run's formatting on top of the default style, appends
/// it to `context.styles`, and returns its style index. Throws on a font
/// reference outside `context.fonts`.
std::uint32_t resolve_style(const TextCFRun &run, StyleContext &context);

} // namespace odr::internal::oldms::presentation
