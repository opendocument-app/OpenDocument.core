#include <gtest/gtest.h>

#include <odr/internal/oldms/text/doc_io.hpp>

#include <sstream>
#include <string>

using namespace odr;

// A compressed (1-byte-per-CP) piece is "an array of 8-bit Unicode characters"
// ([MS-DOC] 2.9.73 / 2.4.1 step 6): each byte is a code point, except the
// 0x82-0x9F values that map to the Windows-1252 punctuation block. The decoder
// must UTF-8-encode the code point, not emit the raw byte (which is invalid
// UTF-8 for 0xA0-0xFF).
TEST(OldMs, doc_read_string_compressed) {
  using internal::oldms::text::read_string_compressed;

  // ASCII: byte == code point == single UTF-8 byte.
  {
    std::istringstream in(std::string("Hi!"));
    EXPECT_EQ(read_string_compressed(in, 3), "Hi!");
  }

  // Mapped byte: 0x92 -> U+2019 (RIGHT SINGLE QUOTATION MARK) -> UTF-8 E2
  // 80 99.
  {
    std::istringstream in(std::string("\x92", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xE2\x80\x99");
  }

  // High Latin-1 byte not in the table: 0xE9 -> U+00E9 ('é') -> UTF-8 C3 A9.
  // This is the case the old `push_back` got wrong (it emitted the lone 0xE9).
  {
    std::istringstream in(std::string("\xE9", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xC3\xA9");
  }

  // Mixed run: "café" stored as A-S 'c''a''f' + 0xE9.
  {
    std::istringstream in(std::string("caf\xE9", 4));
    EXPECT_EQ(read_string_compressed(in, 4), "caf\xC3\xA9");
  }
}
