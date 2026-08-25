#include <odr/internal/iwork/iwork_protobuf.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::iwork;

namespace {

std::string varint(std::uint64_t value) {
  std::string result;
  for (;;) {
    const auto byte = static_cast<char>(value & 0x7f);
    value >>= 7;
    result.push_back(value == 0 ? byte : static_cast<char>(byte | 0x80));
    if (value == 0) {
      return result;
    }
  }
}

std::string key(const std::uint32_t number, const WireType type) {
  return varint((number << 3) | static_cast<std::uint64_t>(type));
}

std::string length_delimited(const std::uint32_t number,
                             const std::string &bytes) {
  return key(number, WireType::length_delimited) + varint(bytes.size()) + bytes;
}

void parse(const std::string &data) {
  const Message message(data);
  (void)message;
}

} // namespace

TEST(ProtobufMessage, varint_field) {
  const std::string data = key(1, WireType::varint) + varint(10000);
  const Message message(data);
  EXPECT_EQ(message.number_field(1), 10000);
}

// The wire format's largest varint is ten bytes.
TEST(ProtobufMessage, largest_varint) {
  const std::string data = key(1, WireType::varint) +
                           varint(std::numeric_limits<std::uint64_t>::max());
  const Message message(data);
  EXPECT_EQ(message.number_field(1), std::numeric_limits<std::uint64_t>::max());
}

TEST(ProtobufMessage, varint_does_not_terminate) {
  EXPECT_ANY_THROW(parse(key(1, WireType::varint) + std::string(11, '\xff')));
}

TEST(ProtobufMessage, fixed_fields) {
  const std::string data =
      key(1, WireType::fixed32) + std::string{'\x04', '\x03', '\x02', '\x01'} +
      key(2, WireType::fixed64) + std::string{'\x08', '\x07', '\x06', '\x05',
                                              '\x04', '\x03', '\x02', '\x01'};
  const Message message(data);
  EXPECT_EQ(message.number_field(1), 0x01020304);
  EXPECT_EQ(message.number_field(2), 0x0102030405060708);
}

// A geometry's `float`s are `fixed32`; a field of any other wire type is not
// one, which matters where a field number was guessed rather than read.
TEST(ProtobufMessage, float_field) {
  const std::string data = key(1, WireType::fixed32) +
                           std::string{'\x00', '\x00', '\x80', '\xbf'} +
                           key(2, WireType::varint) + varint(1);
  const Message message(data);
  EXPECT_EQ(message.float_field(1), -1.0F);
  EXPECT_FALSE(message.float_field(2).has_value());
  EXPECT_FALSE(message.float_field(3).has_value());
}

TEST(ProtobufMessage, bytes_field) {
  const std::string data = length_delimited(3, "Table of Contents");
  const Message message(data);
  EXPECT_EQ(message.bytes_field(3), "Table of Contents");
}

TEST(ProtobufMessage, nested_message) {
  const std::string data =
      length_delimited(2, key(1, WireType::varint) + varint(1732588));
  const Message message(data);
  const Message nested(message.bytes_field(2).value());
  EXPECT_EQ(nested.number_field(1), 1732588);
}

TEST(ProtobufMessage, repeated_field) {
  const std::string data = length_delimited(3, "a") + length_delimited(4, "b") +
                           length_delimited(3, "c");
  const Message message(data);
  const std::vector<Field> repeated = message.repeated_field(3);
  ASSERT_EQ(repeated.size(), 2);
  EXPECT_EQ(repeated[0].bytes, "a");
  EXPECT_EQ(repeated[1].bytes, "c");
}

// A field we have no accessor for is read like any other and left alone.
TEST(ProtobufMessage, unknown_field_is_kept) {
  const std::string data =
      key(999, WireType::varint) + varint(1) + length_delimited(3, "text");
  const Message message(data);
  EXPECT_EQ(message.fields().size(), 2);
  EXPECT_EQ(message.bytes_field(3), "text");
  EXPECT_FALSE(message.bytes_field(999).has_value());
  EXPECT_FALSE(message.number_field(3).has_value());
}

TEST(ProtobufMessage, absent_field) {
  const std::string data = length_delimited(3, "text");
  const Message message(data);
  EXPECT_FALSE(message.field(4).has_value());
  EXPECT_TRUE(message.repeated_field(4).empty());
}

TEST(ProtobufMessage, field_runs_past_the_message) {
  EXPECT_ANY_THROW(
      parse(key(3, WireType::length_delimited) + varint(10) + "short"));
}

// Groups are deprecated and no iWork archive carries one, so reading one means
// the parse went wrong rather than that a field needs skipping.
TEST(ProtobufMessage, group_field) {
  EXPECT_ANY_THROW(parse(key(1, WireType::start_group)));
}

TEST(ProtobufMessage, field_number_zero) {
  EXPECT_ANY_THROW(parse(key(0, WireType::varint) + varint(1)));
}

TEST(ReadVarint, advances_past_the_field) {
  const std::string data = varint(300) + varint(1);
  std::size_t position = 0;
  EXPECT_EQ(read_varint(data, position), 300);
  EXPECT_EQ(position, 2);
  EXPECT_EQ(read_varint(data, position), 1);
  EXPECT_EQ(position, 3);
}
