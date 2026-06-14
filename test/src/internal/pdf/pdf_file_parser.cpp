#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <test_util.hpp>

#include <memory>
#include <ranges>

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
        std::string stream = parser.read_stream(-1);
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
