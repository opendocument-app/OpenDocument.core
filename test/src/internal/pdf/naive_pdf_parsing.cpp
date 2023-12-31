#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_parser.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <test_util.hpp>

#include <iostream>
#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::test;

TEST(NaivePdfParsing, foo) {
  // useful readings
  // https://www.oreilly.com/library/view/pdf-explained/9781449321581/ch04.html
  // https://resources.infosecinstitute.com/topics/hacking/pdf-file-format-basic-structure/

  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/empty.pdf"));

  std::cout << *file->disk_path() << std::endl;
  std::cout << file->size() << std::endl;

  std::string content = util::stream::read(*file->stream());

  std::cout << content.size() << std::endl;
  std::cout << content << std::endl;

  auto in = file->stream();
  pdf::parser::read_header(*in);
  while (!in->eof()) {
    pdf::parser::read_entry(*in);
  }
}
