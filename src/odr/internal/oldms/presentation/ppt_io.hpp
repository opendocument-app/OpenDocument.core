#pragma once

#include <odr/style.hpp>

#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::presentation {

// NOTE: these helpers read multi-byte values in host byte order, so they are
// correct only on little-endian hosts. Handling big-endian is a future task.

RecordHeader read_record_header(std::istream &in);
CurrentUserAtomHead read_current_user_atom_head(std::istream &in);
UserEditAtomBody read_user_edit_atom_body(std::istream &in);

/// Reads a raw unsigned 32-bit integer (an offset/identifier). See the
/// byte-order note above.
std::uint32_t read_u32(std::istream &in);

/// Decodes a TextCharsAtom body (rec_len bytes, two per UTF-16 code unit) to
/// UTF-8. See the byte-order note above.
std::string read_text_chars(std::istream &in, std::uint32_t rec_len);

/// Decodes a TextBytesAtom body (rec_len bytes, one byte per character, each in
/// the range 0x00-0xFF) to UTF-8.
std::string read_text_bytes(std::istream &in, std::uint32_t rec_len);

/// Reads an OfficeArtClientAnchor into {top, left, right, bottom}: rec_len 8 →
/// SmallRectStruct (int16), 16 → RectStruct (int32). Throws on any other.
Anchor read_client_anchor(std::istream &in, std::uint32_t rec_len);

/// Reads a TextCharsAtom body without decoding (rec_len / 2 UTF-16 units).
std::u16string read_raw_text_chars(std::istream &in, std::uint32_t rec_len);

/// Reads a TextBytesAtom body without decoding (rec_len characters).
std::string read_raw_text_bytes(std::istream &in, std::uint32_t rec_len);

/// Decodes TextBytesAtom characters (each byte a code point 0x00-0xFF) to
/// UTF-8.
std::string decode_text_bytes(std::string_view bytes);

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

} // namespace odr::internal::oldms::presentation
