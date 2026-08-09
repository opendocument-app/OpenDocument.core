#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/encoding/detect.hpp>
#include <odr/internal/encoding/transcode.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>

using namespace odr;
using namespace odr::internal;

TEST(TextEncoding, every_encoding_has_a_name) {
  for (const TextEncoding text_encoding : all_text_encodings()) {
    EXPECT_FALSE(text_encoding_to_string(text_encoding).empty());
    EXPECT_FALSE(text_encoding_names(text_encoding).empty());
  }
}

TEST(TextEncoding, unknown_has_no_name) {
  EXPECT_THROW((void)text_encoding_to_string(TextEncoding::unknown),
               UnsupportedTextEncoding);
  EXPECT_TRUE(text_encoding_names(TextEncoding::unknown).empty());
  EXPECT_FALSE(text_encoding_is_decodable(TextEncoding::unknown));
}

/// The lookups take the first match, so a name on two encodings would make one
/// of them unreachable.
TEST(TextEncoding, no_name_is_claimed_twice) {
  std::set<std::string> seen;
  for (const TextEncoding text_encoding : all_text_encodings()) {
    for (const std::string_view name : text_encoding_names(text_encoding)) {
      std::string normalized;
      for (const char c : name) {
        if (c != '-' && c != '_') {
          normalized.push_back(
              static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
      }
      EXPECT_TRUE(seen.insert(normalized).second)
          << "name claimed twice: " << name;
    }
  }
}

TEST(TextEncoding, every_name_finds_its_encoding) {
  for (const TextEncoding text_encoding : all_text_encodings()) {
    for (const std::string_view name : text_encoding_names(text_encoding)) {
      EXPECT_EQ(text_encoding_by_name(name), text_encoding) << name;
    }
  }
}

TEST(TextEncoding, a_name_matches_regardless_of_case_and_punctuation) {
  EXPECT_EQ(text_encoding_by_name("windows-1252"), TextEncoding::windows_1252);
  EXPECT_EQ(text_encoding_by_name("WINDOWS_1252"), TextEncoding::windows_1252);
  EXPECT_EQ(text_encoding_by_name("cp1252"), TextEncoding::windows_1252);
  EXPECT_EQ(text_encoding_by_name("utf8"), TextEncoding::utf8);
  EXPECT_EQ(text_encoding_by_name("Latin1"), TextEncoding::iso_8859_1);
  EXPECT_EQ(text_encoding_by_name("nonsense"), TextEncoding::unknown);
}

TEST(TextEncoding, uchardet_names_map_home) {
  // the spellings uchardet reports, which are what `detect` has to resolve
  for (const std::string_view name :
       {"UTF-8", "ASCII", "ISO-8859-1", "WINDOWS-1252", "KOI8-R", "SHIFT_JIS",
        "EUC-JP", "Big5", "IBM866", "TIS-620"}) {
    EXPECT_NE(text_encoding_by_name(name), TextEncoding::unknown) << name;
  }
}

TEST(Transcode, single_byte) {
  EXPECT_EQ(encoding::to_utf8("caf\xe9", TextEncoding::iso_8859_1), "café");
  EXPECT_EQ(encoding::to_utf8("caf\xe9", TextEncoding::windows_1252), "café");
  // the euro sits where latin-1 has nothing
  EXPECT_EQ(encoding::to_utf8("\x80", TextEncoding::windows_1252), "€");
  EXPECT_EQ(encoding::to_utf8("\xa4", TextEncoding::iso_8859_15), "€");
}

/// windows-1252 leaves a handful of codes undefined; they are not an error.
TEST(Transcode, an_undefined_byte_becomes_the_replacement_character) {
  EXPECT_EQ(encoding::to_utf8("\x81", TextEncoding::windows_1252), "�");
}

TEST(Transcode, utf8_passes_through_and_repairs) {
  EXPECT_EQ(encoding::to_utf8("café", TextEncoding::utf8), "café");
  EXPECT_EQ(encoding::to_utf8("\xef\xbb\xbfhi", TextEncoding::utf8), "hi");
  EXPECT_EQ(encoding::to_utf8("a\xff"
                              "b",
                              TextEncoding::utf8),
            "a�b");
}

TEST(Transcode, utf16) {
  EXPECT_EQ(encoding::to_utf8(std::string("h\0i\0", 4), TextEncoding::utf16le),
            "hi");
  EXPECT_EQ(encoding::to_utf8(std::string("\0h\0i", 4), TextEncoding::utf16be),
            "hi");
  EXPECT_EQ(encoding::to_utf8(std::string("\xff\xfeh\0i\0", 6),
                              TextEncoding::utf16le),
            "hi");
  // a surrogate pair, U+1F600
  EXPECT_EQ(encoding::to_utf8(std::string("\x3d\xd8\x00\xde", 4),
                              TextEncoding::utf16le),
            "😀");
}

TEST(Transcode, a_broken_utf16_unit_becomes_the_replacement_character) {
  // an unpaired high surrogate, and a trailing odd byte
  EXPECT_EQ(
      encoding::to_utf8(std::string("\x3d\xd8h\0", 4), TextEncoding::utf16le),
      "�h");
  EXPECT_EQ(encoding::to_utf8(std::string("h\0!", 3), TextEncoding::utf16le),
            "h�");
}

TEST(Transcode, utf32) {
  EXPECT_EQ(encoding::to_utf8(std::string("h\0\0\0i\0\0\0", 8),
                              TextEncoding::utf32le),
            "hi");
  EXPECT_EQ(encoding::to_utf8(std::string("\0\0\0h\0\0\0i", 8),
                              TextEncoding::utf32be),
            "hi");
}

/// A mark is only a mark for the encoding that defines it — read as
/// windows-1252 those same bytes are three real characters.
TEST(Transcode, a_byte_order_mark_is_stripped_only_by_its_own_encoding) {
  EXPECT_EQ(encoding::to_utf8("\xef\xbb\xbf", TextEncoding::utf8), "");
  EXPECT_EQ(encoding::to_utf8("\xef\xbb\xbf", TextEncoding::windows_1252),
            "ï»¿");
}

TEST(Transcode, an_undecodable_encoding_throws) {
  EXPECT_THROW((void)encoding::to_utf8("x", TextEncoding::shift_jis),
               std::runtime_error);
  EXPECT_THROW((void)encoding::to_utf8("x", TextEncoding::unknown),
               std::runtime_error);
}

TEST(Detect, a_byte_order_mark_wins) {
  EXPECT_EQ(encoding::detect("\xef\xbb\xbfhello"), TextEncoding::utf8);
  EXPECT_EQ(encoding::detect(std::string("\xff\xfe\0\0", 4)),
            TextEncoding::utf32le);
  EXPECT_EQ(encoding::detect(std::string("\xff\xfeh\0", 4)),
            TextEncoding::utf16le);
}
