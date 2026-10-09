#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/json/json_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace odr;

TEST(JsonFile, unicode_byte_order_marks_are_decoded_before_detection) {
  const std::string source = R"({"key":"value"})";
  for (const std::uint32_t width : {2u, 4u}) {
    for (const bool little : {false, true}) {
      std::string bytes;
      const auto append = [&](const std::uint32_t value) {
        for (std::uint32_t i = 0; i < width; ++i) {
          const std::uint32_t shift = 8 * (little ? i : width - i - 1);
          bytes += static_cast<char>((value >> shift) & 0xff);
        }
      };
      append(0xfeff);
      for (const char c : source) {
        append(static_cast<std::uint8_t>(c));
      }
      const File file = File::from_memory(bytes, "sample.json");
      const DecodedFile decoded = open(file);
      EXPECT_EQ(decoded.file_type(), FileType::javascript_object_notation);
      EXPECT_EQ(decoded.as_text_file().text(), source);
      EXPECT_NO_THROW(
          std::ignore = open(
              file, DecodeOptions::as(FileType::javascript_object_notation)));
    }
  }
}

TEST(JsonFile, validation_requires_one_complete_value) {
  for (const std::string source :
       {R"({"a":[1,true,null]})", "42", "\"text\""}) {
    EXPECT_NO_THROW(internal::json::check_json_file(source));
  }
  for (const std::string source : {"", "{", "[1,]", "{}[]", "true false"}) {
    EXPECT_THROW(internal::json::check_json_file(source), NoJsonFile);
  }
}

TEST(JsonFile, malformed_json_falls_back_to_text) {
  const File file = File::from_memory(R"({"a":})", "sample.json");
  EXPECT_EQ(open(file).file_type(), FileType::text_file);
  EXPECT_THROW(
      std::ignore =
          open(file, DecodeOptions::as(FileType::javascript_object_notation)),
      NoJsonFile);
}
