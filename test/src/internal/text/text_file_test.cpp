#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/text/text_file.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

using namespace odr;
using namespace odr::test;

namespace {

internal::text::TextFile text_file(const std::string &content) {
  return internal::text::TextFile(File::from_memory(content).impl());
}

} // namespace

TEST(TextFile, txt) {
  File(TestData::test_file_path("odr-public/txt/lorem ipsum.txt"));
}

/// Text is the fallback, so it is the one place that can call a file
/// unreadable — otherwise every caller re-derives that from the encoding.
TEST(TextFile, binary_is_not_text) {
  // every byte value twice; a short random run is named or not by luck
  std::string all_bytes;
  for (std::uint32_t i = 0; i < 512; ++i) {
    all_bytes.push_back(static_cast<char>(i % 256));
  }
  EXPECT_THROW(text_file(all_bytes), NoTextFile);

  // uchardet names nul padding utf-8, so the encoding alone does not decide
  EXPECT_THROW(
      text_file(std::string("SQLite format 3\0", 16) + std::string(496, '\0')),
      NoTextFile);
  EXPECT_THROW(text_file(std::string(512, '\0')), NoTextFile);

  const File odt(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_THROW(internal::text::TextFile(odt.impl()), NoTextFile);
}

TEST(TextFile, text_is_text) {
  EXPECT_NO_THROW(text_file("plain ascii"));
  // an empty file is an empty text file
  EXPECT_NO_THROW(text_file(""));
  // utf-16 and utf-32 spell ascii with nul bytes
  EXPECT_NO_THROW(text_file(std::string("\xff\xfeh\0e\0l\0l\0o\0", 12)));
  EXPECT_NO_THROW(text_file(std::string("\xff\xfe\0\0h\0\0\0i\0\0\0", 12)));
}

/// The same contract seen from outside, which `wasm/tests/smoke.test.mjs`
/// pins downstream: bytes nothing recognises do not open at all.
TEST(TextFile, unrecognised_bytes_do_not_open) {
  const File junk = File::from_memory(std::string(512, '\0'));

  EXPECT_THROW(std::ignore = mimetype(junk), UnknownFileType);
  EXPECT_THROW(DecodedFile{junk}, UnknownFileType);
  // asking for text by name is no way around it
  EXPECT_THROW(DecodedFile(junk, FileType::text_file), UnknownFileType);
}

TEST(TextFile, encoding_comes_from_the_byte_order_mark) {
  EXPECT_EQ(text_file("\xef\xbb\xbfhello").encoding(), TextEncoding::utf8);
  EXPECT_EQ(text_file(std::string("\xff\xfeh\0i\0", 6)).encoding(),
            TextEncoding::utf16le);
  EXPECT_EQ(text_file(std::string("\xfe\xff\0h\0i", 6)).encoding(),
            TextEncoding::utf16be);
  EXPECT_EQ(text_file(std::string("\xff\xfe\0\0h\0\0\0", 8)).encoding(),
            TextEncoding::utf32le);
}

TEST(TextFile, text_is_decoded_to_utf8) {
  // 0xe9 is `é` in latin-1 and not valid utf-8, so the encoding decides
  const TextFile file(std::make_shared<internal::text::TextFile>(
      File::from_memory("caf\xe9").impl(), TextEncoding::iso_8859_1));
  EXPECT_EQ(file.text(), "café");
}

/// Nothing decodes these, so the bytes come back as they are and the caller
/// hands them to something that does.
TEST(TextFile, an_undecodable_encoding_yields_its_bytes) {
  const std::string content = "\x82\xa0\x82\xa2";
  const TextFile file(std::make_shared<internal::text::TextFile>(
      File::from_memory(content).impl(), TextEncoding::shift_jis));
  EXPECT_FALSE(text_encoding_is_decodable(TextEncoding::shift_jis));
  EXPECT_EQ(file.text(), content);
}
