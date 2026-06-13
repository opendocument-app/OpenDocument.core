#include <odr/internal/pdf/pdf_file_object.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

TEST(XrefStreamTable, used_and_compressed_entries) {
  // W [1 2 1]: type, two-byte big-endian offset, generation
  const std::string data("\x01\x0e\x8a\x00"
                         "\x02\x00\x02\x05",
                         8);

  const Xref xref = parse_xref_stream_table(data, {1, 2, 1}, {{23, 2}});

  ASSERT_EQ(xref.table.size(), 2);

  const Xref::Entry &used = xref.table.at(ObjectReference(23, 0));
  ASSERT_TRUE(used.is_used());
  EXPECT_EQ(used.as_used().position, 0x0e8a);

  // type 2 entries always have generation 0
  const Xref::Entry &compressed = xref.table.at(ObjectReference(24, 0));
  ASSERT_TRUE(compressed.is_compressed());
  EXPECT_EQ(compressed.as_compressed().stream_id, 2);
  EXPECT_EQ(compressed.as_compressed().index, 5);
}

TEST(XrefStreamTable, free_entry) {
  const std::string data("\x00\x00\x07\x03", 4);

  const Xref xref = parse_xref_stream_table(data, {1, 2, 1}, {{4, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(4, 3));
  ASSERT_TRUE(entry.is_free());
  EXPECT_EQ(entry.as_free().next_free_id, 7);
  EXPECT_EQ(entry.as_free().next_generation, 3);
}

TEST(XrefStreamTable, type_field_width_zero_defaults_to_used) {
  const std::string data("\x0e\x8a\x00", 3);

  const Xref xref = parse_xref_stream_table(data, {0, 2, 1}, {{7, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(7, 0));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x0e8a);
}

TEST(XrefStreamTable, generation_field_width_zero_defaults_to_zero) {
  const std::string data("\x01\x00\x42", 3);

  const Xref xref = parse_xref_stream_table(data, {1, 2, 0}, {{7, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(7, 0));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x42);
}

TEST(XrefStreamTable, big_endian_wide_fields) {
  const std::string data("\x01\x00\x01\x02\x03\x00\x05", 7);

  const Xref xref = parse_xref_stream_table(data, {1, 4, 2}, {{1, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(1, 5));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x00010203);
}

TEST(XrefStreamTable, multiple_subsections) {
  const std::string data("\x01\x00\x10\x00"
                         "\x01\x00\x20\x00"
                         "\x01\x00\x30\x00",
                         12);

  const Xref xref = parse_xref_stream_table(data, {1, 2, 1}, {{3, 1}, {10, 2}});

  ASSERT_EQ(xref.table.size(), 3);
  EXPECT_EQ(xref.table.at(ObjectReference(3, 0)).as_used().position, 0x10);
  EXPECT_EQ(xref.table.at(ObjectReference(10, 0)).as_used().position, 0x20);
  EXPECT_EQ(xref.table.at(ObjectReference(11, 0)).as_used().position, 0x30);
}

TEST(XrefStreamTable, unknown_entry_type_is_absent) {
  const std::string data("\x03\x00\x10\x00", 4);

  const Xref xref = parse_xref_stream_table(data, {1, 2, 1}, {{1, 1}});

  EXPECT_TRUE(xref.table.empty());
}

TEST(XrefStreamTable, exhausted_data_throws) {
  const std::string data("\x01\x0e", 2);

  EXPECT_THROW((void)parse_xref_stream_table(data, {1, 2, 1}, {{1, 1}}),
               std::runtime_error);
}

TEST(ObjectStream, parse_members) {
  // two members: object 11 (a dictionary) at relative offset 0, object 12
  // (an array) at relative offset 9; /First is 10 (the header length)
  const std::string data = "11 0 12 9\n<</A 1>> [1 2 3]";

  std::istringstream in(data);
  const std::vector<ObjectStreamMember> members =
      parse_object_stream(in, 2, 10);

  ASSERT_EQ(members.size(), 2);
  EXPECT_EQ(members[0].id, 11);
  EXPECT_EQ(members[1].id, 12);

  ASSERT_TRUE(members[0].object.is_dictionary());
  EXPECT_EQ(members[0].object.as_dictionary()["A"].as_integer(), 1);

  ASSERT_TRUE(members[1].object.is_array());
  ASSERT_EQ(members[1].object.as_array().size(), 3);
  EXPECT_EQ(members[1].object.as_array()[2].as_integer(), 3);
}

TEST(Xref, append_keeps_newest_entry) {
  Xref newer;
  newer.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{100});

  Xref older;
  older.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{50});
  older.table.emplace(ObjectReference(2, 0), Xref::UsedEntry{60});

  newer.append(older);

  EXPECT_EQ(newer.table.at(ObjectReference(1, 0)).as_used().position, 100);
  EXPECT_EQ(newer.table.at(ObjectReference(2, 0)).as_used().position, 60);
}

TEST(Xref, merge_hybrid_fills_absent_and_free_only) {
  Xref classic;
  classic.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{100});
  classic.table.emplace(ObjectReference(2, 0), Xref::FreeEntry{0, 0});
  classic.table.emplace(ObjectReference(5, 65535), Xref::FreeEntry{0, 65535});

  Xref stream;
  stream.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{999});
  stream.table.emplace(ObjectReference(2, 0), Xref::CompressedEntry{7, 0});
  stream.table.emplace(ObjectReference(3, 0), Xref::UsedEntry{300});
  stream.table.emplace(ObjectReference(5, 0), Xref::UsedEntry{500});

  classic.merge_hybrid(stream);

  // the classic in-use entry wins over the stream's
  EXPECT_EQ(classic.table.at(ObjectReference(1, 0)).as_used().position, 100);
  // hidden object: free in the classic table, live in the stream
  EXPECT_TRUE(classic.table.at(ObjectReference(2, 0)).is_compressed());
  // absent in the classic table
  EXPECT_EQ(classic.table.at(ObjectReference(3, 0)).as_used().position, 300);
  // free under a different generation key
  EXPECT_EQ(classic.table.at(ObjectReference(5, 0)).as_used().position, 500);
}
