#pragma once

#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>

namespace odr::internal::oldms::presentation {

// NOTE: these helpers copy the file's bytes straight into native integers and
// code units, so they interpret multi-byte values in the host's byte order.
// That is correct only while the host's byte order matches the file's, which is
// what this module currently relies on. Handling a mismatch (detect it, then
// swap the inputs before they reach these readers, or add a swap flag here) is
// a future task and intentionally out of scope for now.

void read(std::istream &in, RecordHeader &out);
void read(std::istream &in, CurrentUserAtomHead &out);
void read(std::istream &in, UserEditAtomBody &out);

// Reads a raw unsigned 32-bit integer (an offset/identifier). See the
// byte-order note above.
std::uint32_t read_u32(std::istream &in);

// Decodes a TextCharsAtom body (rec_len bytes, two per UTF-16 code unit) to
// UTF-8. See the byte-order note above.
std::string read_text_chars(std::istream &in, std::uint32_t rec_len);

// Decodes a TextBytesAtom body (rec_len bytes, one byte per character, each in
// the range 0x00-0xFF) to UTF-8.
std::string read_text_bytes(std::istream &in, std::uint32_t rec_len);

// Reads an OfficeArtClientAnchor body into {top, left, right, bottom}: rec_len
// 8 → SmallRectStruct (signed int16), rec_len 16 → RectStruct (signed int32),
// in that field order. Returns nullopt (reading nothing) for any other rec_len.
std::optional<Anchor> read_client_anchor(std::istream &in,
                                         std::uint32_t rec_len);

} // namespace odr::internal::oldms::presentation
