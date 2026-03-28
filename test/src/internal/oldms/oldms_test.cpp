#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/archive.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <odr/internal/oldms/word/io.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

using namespace odr;
using namespace odr::test;

TEST(OldMs, test) {
  const auto path = TestData::test_file_path("odr-public/doc/11KB.doc");

  const ArchiveFile cbf_file =
      open(path, FileType::compound_file_binary_format).as_archive_file();
  const Archive cbf = cbf_file.archive();
  const Filesystem files = cbf.as_filesystem();

  for (const auto walker = files.file_walker("/"); !walker.end();
       walker.next()) {
    std::cout << walker.path() << std::endl;
  }

  const std::string word_document =
      internal::util::stream::read(*files.open("/WordDocument").stream());
  std::cout << word_document << std::endl;

  auto stream = files.open("/WordDocument").stream();
  internal::oldms::Fib fib;
  internal::oldms::read(*stream, fib);
}
