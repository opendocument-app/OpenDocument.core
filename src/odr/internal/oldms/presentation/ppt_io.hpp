#pragma once

#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <string>

namespace odr::internal::oldms::presentation {

void read(std::istream &in, RecordHeader &out);

// Decodes a TextCharsAtom body (rec_len bytes of UTF-16LE) to UTF-8.
std::string read_text_chars(std::istream &in, std::uint32_t rec_len);

// Decodes a TextBytesAtom body (rec_len bytes, each the low byte of a UTF-16
// code unit with a zero high byte) to UTF-8.
std::string read_text_bytes(std::istream &in, std::uint32_t rec_len);

} // namespace odr::internal::oldms::presentation
