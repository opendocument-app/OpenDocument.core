#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <test_util.hpp>

#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;

TEST(FileParser, foo) {
  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  auto in = file->stream();
  FileParser parser(*in);

  parser.read_header();
  while (true) {
    Entry entry = parser.read_entry();

    if (entry.is_eof()) {
      break;
    }
    if (entry.is_trailer()) {
      const Trailer &trailer = entry.as_trailer();

      for (const auto &[key, value] : trailer.dictionary) {
        std::cout << key << std::endl;
      }
    }
    if (entry.is_object()) {
      const IndirectObject &object = entry.as_object();

      std::cout << "object " << object.reference.id << " "
                << object.reference.gen << std::endl;

      if (object.object.is_integer()) {
        std::cout << "integer " << object.object.as_integer() << std::endl;
      }
      if (object.object.is_array()) {
        std::cout << "array size " << object.object.as_array().size()
                  << std::endl;
      }
      if (object.object.is_dictionary()) {
        const auto &dictionary = object.object.as_dictionary();
        std::cout << "dictionary size " << dictionary.size() << std::endl;
        for (const auto &[key, value] : dictionary) {
          std::cout << key << std::endl;
        }
      }

      if (object.has_stream) {
        const Dictionary &dictionary = object.object.as_dictionary();
        std::string stream = parser.read_stream(-1);
        std::cout << stream.size() << std::endl;

        if (dictionary.has_key("Filter")) {
          const std::string &filter = dictionary["Filter"].as_string();
          std::cout << filter << std::endl;
          if (filter == "FlateDecode") {
            std::string inflated = crypto::util::zlib_inflate(stream);
            std::cout << inflated.size() << std::endl;
          }
        }
      }

      std::cout << std::endl;
    }
  }
}

TEST(FileParser, bar) {
  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  auto in = file->stream();
  FileParser parser(*in);

  parser.read_header();
  parser.seek_start_xref();
  StartXref startxref = parser.read_start_xref();
  parser.in().seekg(startxref.start);

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

  std::cout << "xref" << std::endl;
  for (const auto &entry : xref.table) {
    std::cout << entry.first << " " << entry.second.position << " "
              << entry.second.in_use << std::endl;
  }

  std::cout << "trailer" << std::endl;
  for (const auto &[key, value] : trailer) {
    std::cout << key << std::endl;
  }

  ObjectReference root_ref = trailer["Root"].as_reference();
  std::uint32_t root_pos = xref.table.at(root_ref).position;
  parser.in().seekg(root_pos);
  IndirectObject root = parser.read_indirect_object();
  std::cout << "root" << std::endl;
  ObjectReference root_pages_ref =
      root.object.as_dictionary()["Pages"].as_reference();
  std::cout << root_pages_ref.id << " " << root_pages_ref.gen << std::endl;
}
