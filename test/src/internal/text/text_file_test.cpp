#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/text/text_file.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

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
  EXPECT_THROW(std::ignore = open(junk), UnknownFileType);
  // asking for text by name is no way around it, and says so as the text
  // engine rather than as the strategy
  EXPECT_THROW(std::ignore = open(junk, DecodeOptions::as(FileType::text_file)),
               NoTextFile);
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

namespace {

/// The public handle over @p content, which is what the edit surface sits on.
odr::TextFile opened(const std::string &content) {
  return odr::TextFile(std::make_shared<internal::text::TextFile>(
      File::from_memory(content).impl()));
}

std::string edited(const odr::TextFile &file, const std::string &operations) {
  std::ostringstream out;
  file.write_edited(operations, out);
  return std::move(out).str();
}

std::string set_content(const std::string &text) {
  return nlohmann::json{{"version", 2},
                        {"ops", {{{"op", "setContent"}, {"text", text}}}}}
      .dump();
}

} // namespace

TEST(TextFile, an_edit_writes_the_text_it_states) {
  EXPECT_EQ(edited(opened("one\ntwo\n"), set_content("one\nTWO\n")),
            "one\nTWO\n");
}

/// A host that saves without an edit gets the file back as it was.
TEST(TextFile, an_envelope_of_no_ops_writes_the_file_back) {
  EXPECT_EQ(edited(opened("one\ntwo\n"), R"({"version":2,"ops":[]})"),
            "one\ntwo\n");
}

TEST(TextFile, the_last_op_wins) {
  const std::string ops =
      nlohmann::json{{"version", 2},
                     {"ops",
                      {{{"op", "setContent"}, {"text", "first"}},
                       {{"op", "setContent"}, {"text", "second"}}}}}
          .dump();
  EXPECT_EQ(edited(opened("one"), ops), "second");
}

TEST(TextFile, an_unknown_version_or_op_refuses) {
  const odr::TextFile file = opened("one");

  EXPECT_THROW((void)edited(file, R"({"version":1,"ops":[]})"),
               std::invalid_argument);
  EXPECT_THROW((void)edited(file, R"({"version":2,"ops":[{"op":"setText"}]})"),
               std::invalid_argument);
}

/// The view hands undecodable bytes to the browser as they are, so what comes
/// back could not be put back.
TEST(TextFile, a_file_we_cannot_decode_is_not_savable) {
  EXPECT_TRUE(opened("plain ascii").is_savable());

  const odr::TextFile shift_jis(std::make_shared<internal::text::TextFile>(
      File::from_memory(std::string("\x82\xa0\x82\xa2")).impl(),
      TextEncoding::shift_jis));
  EXPECT_FALSE(shift_jis.is_savable());
  std::ostringstream out;
  EXPECT_THROW(shift_jis.write_edited(set_content("x"), out),
               UnsupportedOperation);
}

/// A decodable encoding that is not utf-8 saves, and saves as utf-8.
TEST(TextFile, a_decodable_encoding_saves_as_utf8) {
  const odr::TextFile latin1(std::make_shared<internal::text::TextFile>(
      File::from_memory(std::string("caf\xe9")).impl(),
      TextEncoding::iso_8859_1));
  ASSERT_TRUE(latin1.is_savable());
  EXPECT_EQ(latin1.text(), "caf\u00e9");

  std::ostringstream out;
  latin1.write_edited(R"({"version":2,"ops":[]})", out);
  EXPECT_EQ(std::move(out).str(), "caf\u00e9");
}
