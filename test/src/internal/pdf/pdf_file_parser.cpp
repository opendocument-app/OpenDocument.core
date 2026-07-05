#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <test_util.hpp>

#include <memory>
#include <ranges>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;

TEST(FileParser, foo) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  const auto in = file->stream();
  FileParser parser(*in);

  parser.read_header();
  while (true) {
    Entry entry = parser.read_entry();

    if (entry.is_eof()) {
      break;
    }
    if (entry.is_trailer()) {
      const auto &[size, dictionary] = entry.as_trailer();

      for (const auto &key : dictionary | std::views::keys) {
        std::cout << key << '\n';
      }
    }
    if (entry.is_object()) {
      const IndirectObject &object = entry.as_object();

      std::cout << "object " << object.reference.id << " "
                << object.reference.gen << '\n';

      if (object.object.is_integer()) {
        std::cout << "integer " << object.object.as_integer() << '\n';
      }
      if (object.object.is_array()) {
        std::cout << "array size " << object.object.as_array().size() << '\n';
      }
      if (object.object.is_dictionary()) {
        const auto &dictionary = object.object.as_dictionary();
        std::cout << "dictionary size " << dictionary.size() << '\n';
        for (const auto &key : dictionary | std::views::keys) {
          std::cout << key << '\n';
        }
      }

      if (object.has_stream) {
        const Dictionary &dictionary = object.object.as_dictionary();
        std::string stream = parser.read_stream();
        std::cout << stream.size() << '\n';

        if (dictionary.has_key("Filter")) {
          const std::string &filter = dictionary["Filter"].as_string();
          std::cout << filter << '\n';
          if (filter == "FlateDecode") {
            std::string inflated = crypto::util::zlib_inflate(stream);
            std::cout << inflated.size() << '\n';
          }
        }
      }

      std::cout << '\n';
    }
  }
}

TEST(FileParser, bar) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  const auto in = file->stream();
  FileParser parser(*in);

  parser.read_header();
  parser.seek_start_xref();
  auto [start] = parser.read_start_xref();
  parser.in().seekg(start);

  Dictionary trailer;
  Xref xref;

  while (true) {
    Entry entry = parser.read_entry();

    if (entry.is_eof()) {
      break;
    }
    if (entry.is_trailer()) {
      trailer = entry.as_trailer().dictionary;
    }
    if (entry.is_xref()) {
      xref = entry.as_xref();
    }
  }

  EXPECT_FALSE(xref.table.empty());
  for (const auto &[ref, entry] : xref.table) {
    if (ref.id == 0) {
      EXPECT_TRUE(entry.is_free());
    } else {
      EXPECT_TRUE(entry.is_used());
      EXPECT_GT(entry.as_used().position, 0);
    }
  }

  ASSERT_TRUE(trailer.has_key("Root"));

  const ObjectReference root_ref = trailer["Root"].as_reference();
  const std::uint32_t root_pos = xref.table.at(root_ref).as_used().position;
  parser.in().seekg(root_pos);
  IndirectObject root = parser.read_indirect_object();
  EXPECT_EQ(root.reference.id, root_ref.id);
  EXPECT_EQ(root.reference.gen, root_ref.gen);
  const ObjectReference root_pages_ref =
      root.object.as_dictionary()["Pages"].as_reference();
  EXPECT_TRUE(xref.table.contains(root_pages_ref));
}

// An indirect object whose body is itself an indirect reference (`5 0 obj
// 12 0 R endobj`). The body's `12` is the object number and `0 R` completes
// the reference rather than terminating the object.
TEST(IndirectObject, reference_body) {
  std::istringstream in("5 0 obj 12 0 R endobj");
  FileParser parser(in);

  const IndirectObject object = parser.read_indirect_object();
  EXPECT_EQ(object.reference.id, 5u);
  EXPECT_EQ(object.reference.gen, 0u);
  ASSERT_TRUE(object.object.is_reference());
  EXPECT_EQ(object.object.as_reference().id, 12u);
  EXPECT_EQ(object.object.as_reference().gen, 0u);
}

// read_stream with a known /Length reads exactly that many bytes, then the
// `endstream` and `endobj` keywords.
TEST(ReadStream, known_length) {
  std::istringstream in("hello worldendstream endobj");
  FileParser parser(in);
  EXPECT_EQ(parser.read_stream(11), "hello world");
}

// read_stream without a length scans forward to `endstream`, keeping the raw
// bytes before it (binary data and all) and dropping only the single EOL that
// delimits the keyword (7.3.8.1).
TEST(ReadStream, scans_to_endstream) {
  {
    std::istringstream in("hello world\nendstream\nendobj");
    FileParser parser(in);
    EXPECT_EQ(parser.read_stream(), "hello world");
  }
  {
    // a CRLF delimiter is stripped whole, not left as a stray CR
    std::istringstream in("data\r\nendstream\r\nendobj");
    FileParser parser(in);
    EXPECT_EQ(parser.read_stream(), "data");
  }
  {
    // binary content, including NUL bytes, is preserved verbatim (a line-based
    // scan would mangle it)
    const std::string data("a\000b c\nendstream\nendobj", 22);
    std::istringstream in(data);
    FileParser parser(in);
    EXPECT_EQ(parser.read_stream(), std::string("a\000b c", 5));
  }
  {
    // an `endstream` occurring inside the payload (not followed by `endobj`) is
    // not a false terminator — the scan continues to the real `endstream
    // endobj`
    std::istringstream in("a endstream b\nendstream\nendobj");
    FileParser parser(in);
    EXPECT_EQ(parser.read_stream(), "a endstream b");
  }
}

TEST(XrefStreamTable, used_and_compressed_entries) {
  // W [1 2 1]: type, two-byte big-endian offset, generation
  const std::string data("\x01\x0e\x8a\x00"
                         "\x02\x00\x02\x05",
                         8);
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({1, 2, 1}, {{23, 2}});

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
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({1, 2, 1}, {{4, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(4, 3));
  ASSERT_TRUE(entry.is_free());
  EXPECT_EQ(entry.as_free().next_free_id, 7);
  EXPECT_EQ(entry.as_free().next_generation, 3);
}

TEST(XrefStreamTable, type_field_width_zero_defaults_to_used) {
  const std::string data("\x0e\x8a\x00", 3);
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({0, 2, 1}, {{7, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(7, 0));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x0e8a);
}

TEST(XrefStreamTable, generation_field_width_zero_defaults_to_zero) {
  const std::string data("\x01\x00\x42", 3);
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({1, 2, 0}, {{7, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(7, 0));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x42);
}

TEST(XrefStreamTable, big_endian_wide_fields) {
  const std::string data("\x01\x00\x01\x02\x03\x00\x05", 7);
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({1, 4, 2}, {{1, 1}});

  const Xref::Entry &entry = xref.table.at(ObjectReference(1, 5));
  ASSERT_TRUE(entry.is_used());
  EXPECT_EQ(entry.as_used().position, 0x00010203);
}

TEST(XrefStreamTable, multiple_subsections) {
  const std::string data("\x01\x00\x10\x00"
                         "\x01\x00\x20\x00"
                         "\x01\x00\x30\x00",
                         12);
  std::istringstream in(data);

  const Xref xref =
      FileParser(in).read_xref_stream_table({1, 2, 1}, {{3, 1}, {10, 2}});

  ASSERT_EQ(xref.table.size(), 3);
  EXPECT_EQ(xref.table.at(ObjectReference(3, 0)).as_used().position, 0x10);
  EXPECT_EQ(xref.table.at(ObjectReference(10, 0)).as_used().position, 0x20);
  EXPECT_EQ(xref.table.at(ObjectReference(11, 0)).as_used().position, 0x30);
}

TEST(XrefStreamTable, unknown_entry_type_is_absent) {
  const std::string data("\x03\x00\x10\x00", 4);
  std::istringstream in(data);

  const Xref xref = FileParser(in).read_xref_stream_table({1, 2, 1}, {{1, 1}});

  EXPECT_TRUE(xref.table.empty());
}

TEST(XrefStreamTable, exhausted_data_throws) {
  const std::string data("\x01\x0e", 2);
  std::istringstream in(data);

  EXPECT_THROW((void)FileParser(in).read_xref_stream_table({1, 2, 1}, {{1, 1}}),
               std::runtime_error);
}

TEST(ObjectStream, parse_members) {
  // two members: object 11 (a dictionary) at relative offset 0, object 12
  // (an array) at relative offset 9; /First is 10 (the header length)
  const std::string data = "11 0 12 9\n<</A 1>> [1 2 3]";

  std::istringstream in(data);
  const std::vector<ObjectStreamMember> members =
      FileParser(in).read_object_stream(2, 10);

  ASSERT_EQ(members.size(), 2);
  EXPECT_EQ(members[0].id, 11);
  EXPECT_EQ(members[1].id, 12);

  ASSERT_TRUE(members[0].object.is_dictionary());
  EXPECT_EQ(members[0].object.as_dictionary()["A"].as_integer(), 1);

  ASSERT_TRUE(members[1].object.is_array());
  ASSERT_EQ(members[1].object.as_array().size(), 3);
  EXPECT_EQ(members[1].object.as_array()[2].as_integer(), 3);
}
