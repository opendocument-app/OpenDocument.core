#include <gtest/gtest.h>
#include <internal/html/pdf_file.h>
#include <internal/pdf/pdf_file.h>
#include <odr/file.h>
#include <odr/html.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(PdfFile, open) {
  auto path = TestData::test_file_path("odr-private/pdf/sample.pdf");
  internal::pdf::PdfFile pdf_file(path);
}

TEST(PdfFile, translate) {
  auto path = TestData::test_file_path("odr-private/pdf/sample.pdf");
  auto pdf_file = std::make_shared<internal::pdf::PdfFile>(path);

  internal::html::translate_pdf_file(PdfFile(pdf_file), ".", {});
}
