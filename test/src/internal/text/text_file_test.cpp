#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/text/text_file.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

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

/// Text is the fallback for bytes nothing else claims, so nothing is rejected
/// — a viewer handed a random binary shows junk rather than an error.
TEST(TextFile, anything_reads_as_text) {
  EXPECT_NO_THROW(text_file(std::string("\0\1\2\3", 4)));
  EXPECT_NO_THROW(text_file("plain ascii"));

  const File odt(TestData::test_file_path("odr-public/odt/about.odt"));
  EXPECT_NO_THROW(internal::text::TextFile(odt.impl()));
}

/// The same contract seen from outside, which `wasm/tests/smoke.test.mjs`
/// pins downstream: bytes nothing recognises still open, as text.
TEST(TextFile, unrecognised_bytes_open_as_text) {
  const File junk = File::from_memory(std::string("\0\1\2\3\4\5\6\7", 8));

  EXPECT_EQ(mimetype(junk), "text/plain");
  EXPECT_EQ(DecodedFile(junk).file_type(), FileType::text_file);
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
