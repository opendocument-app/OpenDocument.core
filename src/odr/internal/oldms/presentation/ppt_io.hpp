#pragma once

#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace odr::internal::oldms::presentation {

// These helpers read multi-byte values in host byte order — little-endian
// hosts only, see oldms/AGENTS.md.

RecordHeader read_record_header(std::istream &in);
CurrentUserAtomHead read_current_user_atom_head(std::istream &in);
UserEditAtomBody read_user_edit_atom_body(std::istream &in);

/// Reads a raw unsigned 32-bit integer (an offset/identifier).
std::uint32_t read_u32(std::istream &in);

/// Reads an OfficeArtClientAnchor into {top, left, right, bottom}: rec_len 8 →
/// SmallRectStruct (int16), 16 → RectStruct (int32). Throws on any other.
Anchor read_client_anchor(std::istream &in, std::uint32_t rec_len);

/// Reads a TextCharsAtom body without decoding (rec_len / 2 UTF-16 units);
/// throws unless rec_len is even ([MS-PPT] 2.9.42).
std::u16string read_raw_text_chars(std::istream &in, std::uint32_t rec_len);

/// Reads a TextBytesAtom body without decoding (rec_len characters).
std::string read_raw_text_bytes(std::istream &in, std::uint32_t rec_len);

/// Decodes TextBytesAtom characters (each byte a code point 0x00-0xFF) to
/// UTF-8.
std::string decode_text_bytes(std::string_view bytes);

} // namespace odr::internal::oldms::presentation
