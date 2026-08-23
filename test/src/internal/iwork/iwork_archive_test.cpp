#include <odr/internal/iwork/iwork_archive.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
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

std::string number_field(const std::uint32_t number,
                         const std::uint64_t value) {
  return varint(number << 3) + varint(value);
}

std::string message_field(const std::uint32_t number,
                          const std::string &bytes) {
  return varint((number << 3) | 2) + varint(bytes.size()) + bytes;
}

/// `TSP.ArchiveInfo`: an identifier and one `MessageInfo` per payload message.
std::string archive_info(
    const std::uint64_t identifier,
    const std::vector<std::pair<std::uint32_t, std::size_t>> &messages) {
  std::string result = number_field(1, identifier);
  for (const auto &[type, length] : messages) {
    result += message_field(2, number_field(1, type) + number_field(3, length));
  }
  return result;
}

std::string
object(const std::uint64_t identifier,
       const std::vector<std::pair<std::uint32_t, std::size_t>> &messages,
       const std::string &payload) {
  const std::string info = archive_info(identifier, messages);
  return varint(info.size()) + info + payload;
}

} // namespace

TEST(ReadObjects, one_object) {
  const std::string data = object(1, {{10000, 5}}, "hello");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].identifier, 1);
  EXPECT_EQ(objects[0].type, 10000);
  EXPECT_EQ(objects[0].payload, "hello");
}

TEST(ReadObjects, objects_follow_one_another) {
  const std::string data =
      object(1, {{10000, 5}}, "hello") + object(1732514, {{2001, 5}}, "world");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 2);
  EXPECT_EQ(objects[1].identifier, 1732514);
  EXPECT_EQ(objects[1].type, 2001);
  EXPECT_EQ(objects[1].payload, "world");
}

// An object may hold more than one message; only the first is modelled, and
// the length of the rest is what keeps the reader in step.
TEST(ReadObjects, later_messages_are_skipped) {
  const std::string data = object(1732594, {{6247, 3}, {6247, 3}}, "onetwo") +
                           object(2, {{222, 1}}, "x");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 2);
  EXPECT_EQ(objects[0].type, 6247);
  EXPECT_EQ(objects[0].payload, "one");
  EXPECT_EQ(objects[1].identifier, 2);
}

// There is no spec and no schema registry, so a type we have not mapped is an
// app version we have not seen — the reader keeps it and moves on.
TEST(ReadObjects, unknown_type_is_kept) {
  const std::string data = object(7, {{123456, 1}}, "x");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_EQ(objects[0].type, 123456);
}

TEST(ReadObjects, empty_payload) {
  const std::string data = object(1732550, {{3047, 0}}, "");

  const std::vector<Object> objects = read_objects(data);
  ASSERT_EQ(objects.size(), 1);
  EXPECT_TRUE(objects[0].payload.empty());
}

TEST(ReadObjects, empty_component) { EXPECT_TRUE(read_objects("").empty()); }

TEST(ReadObjects, archive_info_runs_past_the_component) {
  const std::string data = object(1, {{10000, 5}}, "hello");
  EXPECT_ANY_THROW(std::ignore = read_objects(data.substr(0, 4)));
}

TEST(ReadObjects, payload_runs_past_the_component) {
  const std::string data = object(1, {{10000, 500}}, "hello");
  EXPECT_ANY_THROW(std::ignore = read_objects(data));
}
