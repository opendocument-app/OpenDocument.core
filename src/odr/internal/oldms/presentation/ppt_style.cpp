#include <odr/internal/oldms/presentation/ppt_style.hpp>

#include <cstring>
#include <stdexcept>

namespace odr::internal::oldms::presentation {

namespace {

/// Bounds-checked reader over an in-memory record body.
class BodyCursor final {
public:
  explicit BodyCursor(const std::string_view body) : m_body(body) {}

  [[nodiscard]] std::size_t remaining() const { return m_body.size() - m_at; }

  template <typename T> T read() {
    if (remaining() < sizeof(T)) {
      throw std::runtime_error("ppt: truncated StyleTextPropAtom");
    }
    T value;
    std::memcpy(&value, m_body.data() + m_at, sizeof(T));
    m_at += sizeof(T);
    return value;
  }

  void skip(const std::size_t count) {
    if (remaining() < count) {
      throw std::runtime_error("ppt: truncated StyleTextPropAtom");
    }
    m_at += count;
  }

private:
  std::string_view m_body;
  std::size_t m_at{0};
};

/// Skips a TextPFException ([MS-PPT] 2.9.19): every optional field's size is
/// determined by its PFMasks bit; only tabStops is itself variable.
void skip_text_pf_exception(BodyCursor &cursor) {
  const auto masks = cursor.read<std::uint32_t>();
  const auto has = [masks](const int bit) {
    return (masks & (1u << bit)) != 0;
  };

  // Field order per the spec: bulletFlags, bulletChar, bulletFontRef,
  // bulletSize, bulletColor, textAlignment, lineSpacing, spaceBefore,
  // spaceAfter, leftMargin, indent, defaultTabSize, tabStops, fontAlign,
  // wrapFlags, textDirection.
  if ((masks & 0xF) != 0) { // hasBullet, bulletHasFont/Color/Size
    cursor.skip(2);         // bulletFlags
  }
  if (has(7)) {
    cursor.skip(2); // bulletChar
  }
  if (has(4)) {
    cursor.skip(2); // bulletFontRef
  }
  if (has(6)) {
    cursor.skip(2); // bulletSize
  }
  if (has(5)) {
    cursor.skip(4); // bulletColor
  }
  if (has(11)) {
    cursor.skip(2); // textAlignment
  }
  if (has(12)) {
    cursor.skip(2); // lineSpacing
  }
  if (has(13)) {
    cursor.skip(2); // spaceBefore
  }
  if (has(14)) {
    cursor.skip(2); // spaceAfter
  }
  if (has(8)) {
    cursor.skip(2); // leftMargin
  }
  if (has(10)) {
    cursor.skip(2); // indent
  }
  if (has(15)) {
    cursor.skip(2); // defaultTabSize
  }
  if (has(20)) { // tabStops: count + count * TabStop ([MS-PPT] 2.9.23)
    const auto count = cursor.read<std::uint16_t>();
    cursor.skip(std::size_t{count} * 4);
  }
  if (has(16)) {
    cursor.skip(2); // fontAlign
  }
  if ((masks & 0xE0000) != 0) { // charWrap, wordWrap, overflow (bits 17-19)
    cursor.skip(2);             // wrapFlags
  }
  if (has(21)) {
    cursor.skip(2); // textDirection
  }
}

/// Parses a TextCFException ([MS-PPT] 2.9.14) into a TextCFRun's style fields.
void read_text_cf_exception(BodyCursor &cursor, TextCFRun &run) {
  const auto masks = cursor.read<std::uint32_t>();
  const auto has = [masks](const int bit) {
    return (masks & (1u << bit)) != 0;
  };

  // fontStyle exists if any style bit (bold 0, italic 1, underline 2,
  // shadow 4, fehint 5, kumi 7, emboss 9) or fHasStyle (bits 10-13) is set.
  if ((masks & 0x2B7) != 0 || (masks & 0x3C00) != 0) {
    const auto font_style = cursor.read<std::uint16_t>(); // CFStyle, 2.9.16
    if (has(0)) {
      run.bold = (font_style & 0x1) != 0;
    }
    if (has(1)) {
      run.italic = (font_style & 0x2) != 0;
    }
    if (has(2)) {
      run.underline = (font_style & 0x4) != 0;
    }
  }
  if (has(16)) {
    run.font_ref = cursor.read<std::uint16_t>();
  }
  if (has(21)) {
    cursor.skip(2); // oldEAFontRef
  }
  if (has(22)) {
    cursor.skip(2); // ansiFontRef
  }
  if (has(23)) {
    cursor.skip(2); // symbolFontRef
  }
  if (has(17)) {
    run.font_size = cursor.read<std::int16_t>();
  }
  if (has(18)) {
    // ColorIndexStruct ([MS-PPT] 2.12.2): index 0xFE marks an explicit sRGB
    // color; scheme indexes would need the slide's color scheme.
    const auto red = cursor.read<std::uint8_t>();
    const auto green = cursor.read<std::uint8_t>();
    const auto blue = cursor.read<std::uint8_t>();
    const auto index = cursor.read<std::uint8_t>();
    if (index == 0xFE) {
      run.color = Color(red, green, blue);
    }
  }
  if (has(19)) {
    cursor.skip(2); // position
  }
}

} // namespace

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

TextStyle presentation::resolve_style(const TextCFRun &run,
                                      const StyleContext &context) {
  TextStyle style = context.default_style;
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
        context.fonts[*run.font_ref] == nullptr) {
      throw std::runtime_error("ppt: font reference out of range");
    }
    style.font_name = context.fonts[*run.font_ref];
  }
  return style;
}

} // namespace odr::internal::oldms
