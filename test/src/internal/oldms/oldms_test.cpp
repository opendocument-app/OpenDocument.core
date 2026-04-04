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
  std::cout << "/WordDocument size " << word_document.size() << std::endl;

  const std::size_t fib_size = internal::oldms::determine_size_Fib(
      *files.open("/WordDocument").stream());
  std::cout << "Fib size " << fib_size << std::endl;

  const auto stream = files.open("/WordDocument").stream();
  internal::oldms::ParsedFib fib;
  internal::oldms::read(*stream, fib);

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
  internal::oldms::read_Clx(
      *table_stream, internal::oldms::skip_Prc, [&](std::istream &in) {
        if (const int c = in.get(); c != 0x2) {
          throw std::runtime_error("Unexpected input: " + std::to_string(c));
        }
        const std::uint32_t lcb =
            internal::util::byte_stream::read<std::uint32_t>(in);
        std::cout << "lcb " << lcb << std::endl;
        std::string plcPcd = internal::util::stream::read(in, lcb);
        const internal::oldms::PlcPcdMap plc_pcd_map(plcPcd.data(),
                                                     plcPcd.size());
        std::cout << "plc_pcd_map n " << plc_pcd_map.n() << std::endl;
        std::cout << "plc_pcd_map aCP(0) " << plc_pcd_map.aCP(0) << std::endl;
        std::cout << "plc_pcd_map aCP(1) " << plc_pcd_map.aCP(1) << std::endl;
        std::cout << "plc_pcd_map aData(0).fc.fc " << plc_pcd_map.aData(0).fc.fc
                  << std::endl;
        std::cout << "plc_pcd_map aData(0).fc.fCompressed "
                  << plc_pcd_map.aData(0).fc.fCompressed << std::endl;

        const std::size_t first_text_offset = plc_pcd_map.aData(0).fc.fc / 2;
        const std::size_t first_text_length =
            plc_pcd_map.aCP(1) - plc_pcd_map.aCP(0);
        std::cout << "first_text_length " << first_text_length << std::endl;
        std::cout << "first_text_offset " << first_text_offset << std::endl;

        const auto document_stream = files.open("/WordDocument").stream();
        document_stream->seekg(first_text_offset);
        const std::string first_text = internal::oldms::read_string_compressed(
            *document_stream, first_text_length);
        std::cout << "first_text " << first_text << std::endl;
      });
}
