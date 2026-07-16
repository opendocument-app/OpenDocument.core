#include <odr/internal/oldms/presentation/ppt_io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstring>
#include <istream>
#include <stdexcept>
#include <string>

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

presentation::RecordHeader presentation::read_record_header(std::istream &in) {
  RecordHeader result{};
  util::byte_stream::read(in, result);
  return result;
}

presentation::CurrentUserAtomHead
presentation::read_current_user_atom_head(std::istream &in) {
  CurrentUserAtomHead result{};
  util::byte_stream::read(in, result);
  return result;
}

presentation::UserEditAtomBody
presentation::read_user_edit_atom_body(std::istream &in) {
  UserEditAtomBody result{};
  util::byte_stream::read(in, result);
  return result;
}

std::uint32_t presentation::read_u32(std::istream &in) {
  return util::byte_stream::read<std::uint32_t>(in);
}

std::string presentation::read_text_chars(std::istream &in,
                                          const std::uint32_t rec_len) {
  const std::size_t count = rec_len / 2;
  std::u16string buffer;
  buffer.resize(count);
  in.read(reinterpret_cast<char *>(buffer.data()),
          static_cast<std::streamsize>(count * sizeof(char16_t)));
  return util::string::u16string_to_string(buffer);
}

std::string presentation::read_text_bytes(std::istream &in,
                                          const std::uint32_t rec_len) {
  static constexpr auto eof = std::istream::traits_type::eof();

  std::u16string buffer;
  buffer.reserve(rec_len);
  for (std::uint32_t i = 0; i < rec_len; ++i) {
    const auto c = in.get();
    if (c == eof) {
      break;
    }
    buffer.push_back(static_cast<std::uint8_t>(c));
  }
  return util::string::u16string_to_string(buffer);
}

presentation::Anchor
presentation::read_client_anchor(std::istream &in,
                                 const std::uint32_t rec_len) {
  const auto read_rect = [&in]<typename T>(T) -> Anchor {
    Anchor anchor;
    anchor.top = util::byte_stream::read<T>(in);
    anchor.left = util::byte_stream::read<T>(in);
    anchor.right = util::byte_stream::read<T>(in);
    anchor.bottom = util::byte_stream::read<T>(in);
    return anchor;
  };

  if (rec_len == 8) {
    return read_rect(std::int16_t{}); // SmallRectStruct
  }
  if (rec_len == 16) {
    return read_rect(std::int32_t{}); // RectStruct
  }
  throw std::runtime_error("ppt: unexpected OfficeArtClientAnchor length " +
                           std::to_string(rec_len));
}

std::u16string presentation::read_raw_text_chars(std::istream &in,
                                                 const std::uint32_t rec_len) {
  std::u16string buffer;
  buffer.resize(rec_len / 2);
  in.read(reinterpret_cast<char *>(buffer.data()),
          static_cast<std::streamsize>(buffer.size() * sizeof(char16_t)));
  return buffer;
}

std::string presentation::read_raw_text_bytes(std::istream &in,
                                              const std::uint32_t rec_len) {
  std::string buffer;
  buffer.resize(rec_len);
  in.read(buffer.data(), static_cast<std::streamsize>(rec_len));
  buffer.resize(static_cast<std::size_t>(in.gcount()));
  return buffer;
}

std::string presentation::decode_text_bytes(const std::string_view bytes) {
  std::u16string buffer;
  buffer.reserve(bytes.size());
  for (const char c : bytes) {
    buffer.push_back(static_cast<std::uint8_t>(c));
  }
  return util::string::u16string_to_string(buffer);
}

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

} // namespace odr::internal::oldms
