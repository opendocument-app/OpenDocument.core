#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_parser.hpp>

#include <test_util.hpp>

#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;

TEST(NaivePdfParsing, foo) {
  // useful readings
  // https://www.oreilly.com/library/view/pdf-explained/9781449321581/ch04.html
  // https://resources.infosecinstitute.com/topics/hacking/pdf-file-format-basic-structure/

  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/empty.pdf"));

  auto in = file->stream();
  PdfFileParser parser(*in);

  parser.read_header();
  while (true) {
    Entry entry = parser.read_entry();

    if (entry.type() == typeid(Eof)) {
      break;
    }
    if (entry.type() == typeid(IndirectObject)) {
      const auto &object = std::any_cast<IndirectObject>(entry);
      if (object.has_stream) {
        const std::string &stream = *object.stream;
        std::cout << stream.size() << std::endl;
        std::cout << crypto::util::base64_encode(stream) << std::endl;
        // std::cout << crypto::util::inflate(stream) << std::endl;

        std::cout << crypto::util::inflate(crypto::util::base64_decode(
                         "eJwz0DNUKOcqVDBQMNAzMLJQMLU01TMyN1WwMDHUszAzVChK5QrXU"
                         "sjjClQAALcSCK4="))
                  << std::endl;
      }
    }
  }
}
