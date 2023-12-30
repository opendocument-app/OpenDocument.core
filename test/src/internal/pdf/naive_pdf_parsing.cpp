#include <odr/internal/common/file.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <test_util.hpp>

#include <iostream>
#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::test;

TEST(NaivePdfParsing, foo) {
  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/empty.pdf"));

  std::cout << file->size() << std::endl;

  std::string content = util::stream::read(*file->stream());

  std::cout << content.size() << std::endl;
  std::cout << content << std::endl;
}
