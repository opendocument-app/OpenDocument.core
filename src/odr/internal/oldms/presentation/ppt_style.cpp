#include <odr/internal/oldms/presentation/ppt_style.hpp>

#include <odr/internal/util/byte_string.hpp>

#include <utility>

namespace odr::internal::oldms::presentation {

namespace {

using BodyCursor = util::byte_string::Reader;

/// PFMasks bits ([MS-PPT] 2.9.21).
enum PFMask : std::uint32_t {
  PF_HasBullet = 1u << 0,
  PF_BulletHasFont = 1u << 1,
  PF_BulletHasColor = 1u << 2,
  PF_BulletHasSize = 1u << 3,
  PF_BulletFont = 1u << 4,
  PF_BulletColor = 1u << 5,
  PF_BulletSize = 1u << 6,
  PF_BulletChar = 1u << 7,
  PF_LeftMargin = 1u << 8,
  PF_Indent = 1u << 10,
  PF_Align = 1u << 11,
  PF_LineSpacing = 1u << 12,
  PF_SpaceBefore = 1u << 13,
  PF_SpaceAfter = 1u << 14,
  PF_DefaultTabSize = 1u << 15,
  PF_FontAlign = 1u << 16,
  PF_CharWrap = 1u << 17,
  PF_WordWrap = 1u << 18,
  PF_Overflow = 1u << 19,
  PF_TabStops = 1u << 20,
  PF_TextDirection = 1u << 21,
};

/// CFMasks bits ([MS-PPT] 2.9.15).
enum CFMask : std::uint32_t {
  CF_Bold = 1u << 0,
  CF_Italic = 1u << 1,
  CF_Underline = 1u << 2,
  CF_Shadow = 1u << 4,
  CF_Fehint = 1u << 5,
  CF_Kumi = 1u << 7,
  CF_Emboss = 1u << 9,
  CF_HasStyle = 0xFu << 10,
  CF_Typeface = 1u << 16,
  CF_Size = 1u << 17,
  CF_Color = 1u << 18,
  CF_Position = 1u << 19,
  CF_OldEATypeface = 1u << 21,
  CF_AnsiTypeface = 1u << 22,
  CF_SymbolTypeface = 1u << 23,
};

/// CFStyle bits ([MS-PPT] 2.9.16), in a TextCFException's fontStyle field.
enum CFStyleBit : std::uint16_t {
  CFStyle_Bold = 1u << 0,
  CFStyle_Italic = 1u << 1,
  CFStyle_Underline = 1u << 2,
};

/// ColorIndexStruct index marking an explicit sRGB color ([MS-PPT] 2.12.2).
constexpr std::uint8_t srgb_color_index = 0xFE;

/// A read PFMasks/CFMasks value.
struct Masks final {
  std::uint32_t value;

  /// Whether any of `mask`'s bits is set.
  [[nodiscard]] bool has(const std::uint32_t mask) const {
    return (value & mask) != 0;
  }
};

/// Skips a TextPFException ([MS-PPT] 2.9.19): fields in spec order, each
/// present iff its PFMasks bit is set; only tabStops is variable-size.
void skip_text_pf_exception(BodyCursor &cursor) {
  const Masks masks{cursor.read<std::uint32_t>()};

  if (masks.has(PF_HasBullet | PF_BulletHasFont | PF_BulletHasColor |
                PF_BulletHasSize)) {
    cursor.skip(2); // bulletFlags
  }
  if (masks.has(PF_BulletChar)) {
    cursor.skip(2);
  }
  if (masks.has(PF_BulletFont)) {
    cursor.skip(2); // bulletFontRef
  }
  if (masks.has(PF_BulletSize)) {
    cursor.skip(2);
  }
  if (masks.has(PF_BulletColor)) {
    cursor.skip(4);
  }
  if (masks.has(PF_Align)) {
    cursor.skip(2); // textAlignment
  }
  if (masks.has(PF_LineSpacing)) {
    cursor.skip(2);
  }
  if (masks.has(PF_SpaceBefore)) {
    cursor.skip(2);
  }
  if (masks.has(PF_SpaceAfter)) {
    cursor.skip(2);
  }
  if (masks.has(PF_LeftMargin)) {
    cursor.skip(2);
  }
  if (masks.has(PF_Indent)) {
    cursor.skip(2);
  }
  if (masks.has(PF_DefaultTabSize)) {
    cursor.skip(2);
  }
  if (masks.has(PF_TabStops)) { // count + count * TabStop ([MS-PPT] 2.9.23)
    const auto count = cursor.read<std::uint16_t>();
    cursor.skip(std::size_t{count} * 4);
  }
  if (masks.has(PF_FontAlign)) {
    cursor.skip(2);
  }
  if (masks.has(PF_CharWrap | PF_WordWrap | PF_Overflow)) {
    cursor.skip(2); // wrapFlags
  }
  if (masks.has(PF_TextDirection)) {
    cursor.skip(2);
  }
}

/// Parses a TextCFException ([MS-PPT] 2.9.14) into a TextCFRun's style fields.
void read_text_cf_exception(BodyCursor &cursor, TextCFRun &run) {
  const Masks masks{cursor.read<std::uint32_t>()};

  if (masks.has(CF_Bold | CF_Italic | CF_Underline | CF_Shadow | CF_Fehint |
                CF_Kumi | CF_Emboss | CF_HasStyle)) {
    const auto font_style = cursor.read<std::uint16_t>();
    if (masks.has(CF_Bold)) {
      run.bold = (font_style & CFStyle_Bold) != 0;
    }
    if (masks.has(CF_Italic)) {
      run.italic = (font_style & CFStyle_Italic) != 0;
    }
    if (masks.has(CF_Underline)) {
      run.underline = (font_style & CFStyle_Underline) != 0;
    }
  }
  if (masks.has(CF_Typeface)) {
    run.font_ref = cursor.read<std::uint16_t>();
  }
  if (masks.has(CF_OldEATypeface)) {
    cursor.skip(2); // oldEAFontRef
  }
  if (masks.has(CF_AnsiTypeface)) {
    cursor.skip(2); // ansiFontRef
  }
  if (masks.has(CF_SymbolTypeface)) {
    cursor.skip(2); // symbolFontRef
  }
  if (masks.has(CF_Size)) {
    run.font_size = cursor.read<std::int16_t>();
  }
  if (masks.has(CF_Color)) {
    // ColorIndexStruct ([MS-PPT] 2.12.2); scheme indexes would need the
    // slide's color scheme.
    const auto red = cursor.read<std::uint8_t>();
    const auto green = cursor.read<std::uint8_t>();
    const auto blue = cursor.read<std::uint8_t>();
    const auto index = cursor.read<std::uint8_t>();
    if (index == srgb_color_index) {
      run.color = Color(red, green, blue);
    }
  }
  if (masks.has(CF_Position)) {
    cursor.skip(2); // position
  }
}

} // namespace

StyleRegistry::StyleRegistry(std::vector<std::string> font_names,
                             std::vector<TextStyle> styles)
    : m_font_names(std::move(font_names)), m_styles(std::move(styles)) {}

const TextStyle &StyleRegistry::text_style(const std::uint32_t index) const {
  return m_styles.at(index);
}

} // namespace odr::internal::oldms::presentation

namespace odr::internal::oldms {

std::vector<presentation::TextCFRun>
presentation::parse_style_text_prop_atom(const std::string_view body,
                                         const std::size_t char_count) {
  BodyCursor cursor(body);

  // rgTextPFRun: skipped; the counts cover the whole corresponding text.
  for (std::size_t covered = 0;
       covered < char_count && cursor.remaining() > 0;) {
    covered += cursor.read<std::uint32_t>(); // count
    cursor.skip(2);                          // indentLevel
    skip_text_pf_exception(cursor);
  }

  // rgTextCFRun.
  std::vector<TextCFRun> runs;
  for (std::size_t covered = 0;
       covered < char_count && cursor.remaining() > 0;) {
    TextCFRun run;
    run.count = cursor.read<std::uint32_t>();
    read_text_cf_exception(cursor, run);
    covered += run.count;
    runs.push_back(run);
  }
  return runs;
}

TextStyle presentation::default_character_style() {
  TextStyle style;
  style.font_size = Measure(18.0, DynamicUnit("pt"));
  return style;
}

std::uint32_t presentation::resolve_style(const TextCFRun &run,
                                          StyleContext &context) {
  TextStyle style = context.styles.at(default_style_index);
  if (run.bold.has_value()) {
    style.font_weight = *run.bold ? FontWeight::bold : FontWeight::normal;
  }
  if (run.italic.has_value()) {
    style.font_style = *run.italic ? FontStyle::italic : FontStyle::normal;
  }
  if (run.underline.has_value()) {
    style.font_underline = run.underline;
  }
  if (run.font_size.has_value()) {
    style.font_size =
        Measure(static_cast<double>(*run.font_size), DynamicUnit("pt"));
  }
  if (run.color.has_value()) {
    style.font_color = *run.color;
  }
  if (run.font_ref.has_value()) {
    if (*run.font_ref >= context.fonts.size() ||
        context.fonts[*run.font_ref].empty()) {
      throw std::runtime_error("ppt: font reference out of range");
    }
    style.font_name = context.fonts[*run.font_ref].c_str();
  }
  context.styles.push_back(style);
  return static_cast<std::uint32_t>(context.styles.size() - 1);
}

} // namespace odr::internal::oldms
