#include <test_util.hpp>

#include <gtest/gtest.h>

#include <odr/archive.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
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
  std::cout << "/WordDocument size " << word_document.size() << std::endl;

  const std::size_t fib_size = internal::oldms::text::determine_size_Fib(
      *files.open("/WordDocument").stream());
  std::cout << "Fib size " << fib_size << std::endl;

  const auto stream = files.open("/WordDocument").stream();
  internal::oldms::text::ParsedFib fib;
  internal::oldms::text::read(*stream, fib);

  const std::string tableStreamPath =
      fib.base.fWhichTblStm == 1 ? "/1Table" : "/0Table";

  const std::string table =
      internal::util::stream::read(*files.open(tableStreamPath).stream());
  std::cout << tableStreamPath << " size " << table.size() << std::endl;

  const auto table_stream = files.open(tableStreamPath).stream();
  std::cout << "Fib.fibRgFcLcb->clx.fc " << fib.fibRgFcLcb->clx.fc << std::endl;
  std::cout << "Fib.fibRgFcLcb->clx.lcb " << fib.fibRgFcLcb->clx.lcb
            << std::endl;
  table_stream->ignore(fib.fibRgFcLcb->clx.fc);
  const internal::oldms::text::CharacterIndex character_index =
      internal::oldms::text::read_character_index(*table_stream);
  for (const auto &entry : character_index) {
    const auto document_stream = files.open("/WordDocument").stream();
    document_stream->seekg(entry.data_offset);
    const std::string text = internal::oldms::text::read_string_compressed(
        *document_stream, entry.data_length);
    std::cout << "text " << text << std::endl;
  }
}
