#include <odr/file.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

std::shared_ptr<pdf::PdfFile> open_pdf(const std::string &bytes) {
  return std::make_shared<pdf::PdfFile>(std::make_shared<MemoryFile>(bytes));
}

/// A two-page mini-PDF with an `/Info` dictionary. `/Title` is a UTF-16BE
/// (BOM) hex string ("HI"), the rest literal (PDFDocEncoding) strings.
std::string info_mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      .object("<< /Title <FEFF00480049> /Author (Ada Lovelace) "
              "/Producer (odr-test) >>");
  return builder.trailer("/Root 1 0 R /Info 5 0 R").build_classic();
}

} // namespace

// `/Info` document-information strings and the page count surface through
// `file_meta().document_meta`; a `/Title` UTF-16BE string is decoded to UTF-8.
TEST(PdfFile, file_meta_carries_info_and_page_count) {
  const std::shared_ptr<pdf::PdfFile> file = open_pdf(info_mini_pdf());
  const FileMeta meta = file->file_meta();

  EXPECT_EQ(meta.type, FileType::portable_document_format);
  ASSERT_TRUE(meta.document_meta.has_value());
  const DocumentMeta &document_meta = *meta.document_meta;

  EXPECT_EQ(document_meta.document_type, DocumentType::text);
  ASSERT_TRUE(document_meta.entry_count.has_value());
  EXPECT_EQ(*document_meta.entry_count, 2u);

  ASSERT_TRUE(document_meta.title.has_value());
  EXPECT_EQ(*document_meta.title, "HI");
  ASSERT_TRUE(document_meta.author.has_value());
  EXPECT_EQ(*document_meta.author, "Ada Lovelace");
  ASSERT_TRUE(document_meta.producer.has_value());
  EXPECT_EQ(*document_meta.producer, "odr-test");
  EXPECT_FALSE(document_meta.subject.has_value());
  EXPECT_FALSE(document_meta.keywords.has_value());
}

// A file with no `/Info` still reports its page count; the `/Info` strings stay
// unset rather than empty.
TEST(PdfFile, file_meta_without_info) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  const std::string pdf = builder.trailer("/Root 1 0 R").build_classic();

  const std::shared_ptr<pdf::PdfFile> file = open_pdf(pdf);
  const FileMeta meta = file->file_meta();

  ASSERT_TRUE(meta.document_meta.has_value());
  EXPECT_EQ(meta.document_meta->document_type, DocumentType::text);
  ASSERT_TRUE(meta.document_meta->entry_count.has_value());
  EXPECT_EQ(*meta.document_meta->entry_count, 1u);
  EXPECT_FALSE(meta.document_meta->title.has_value());
  EXPECT_FALSE(meta.document_meta->author.has_value());
}
