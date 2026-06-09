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

#include <sstream>
#include <string>

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
    const std::string text = internal::oldms::text::read_string(
        *document_stream, entry.length_cp, entry.is_compressed);
    std::cout << "text " << text << std::endl;
  }
}

// A compressed (1-byte-per-CP) piece is "an array of 8-bit Unicode characters"
// ([MS-DOC] 2.9.73 / 2.4.1 step 6): each byte is a code point, except the
// 0x82-0x9F values that map to the Windows-1252 punctuation block. The decoder
// must UTF-8-encode the code point, not emit the raw byte (which is invalid
// UTF-8 for 0xA0-0xFF).
TEST(OldMs, doc_read_string_compressed) {
  using internal::oldms::text::read_string_compressed;

  // ASCII: byte == code point == single UTF-8 byte.
  {
    std::istringstream in(std::string("Hi!"));
    EXPECT_EQ(read_string_compressed(in, 3), "Hi!");
  }

  // Mapped byte: 0x92 -> U+2019 (RIGHT SINGLE QUOTATION MARK) -> UTF-8 E2
  // 80 99.
  {
    std::istringstream in(std::string("\x92", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xE2\x80\x99");
  }

  // High Latin-1 byte not in the table: 0xE9 -> U+00E9 ('é') -> UTF-8 C3 A9.
  // This is the case the old `push_back` got wrong (it emitted the lone 0xE9).
  {
    std::istringstream in(std::string("\xE9", 1));
    EXPECT_EQ(read_string_compressed(in, 1), "\xC3\xA9");
  }

  // Mixed run: "café" stored as A-S 'c''a''f' + 0xE9.
  {
    std::istringstream in(std::string("caf\xE9", 4));
    EXPECT_EQ(read_string_compressed(in, 4), "caf\xC3\xA9");
  }
}
