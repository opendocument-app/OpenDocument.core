#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>

#include <test_util.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

std::string two_object_mini_pdf(const bool classic) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R");
  return classic ? builder.build_classic() : builder.build_xref_stream();
}

void check_mini_pdf(const std::string &pdf) {
  std::istringstream in(pdf);
  DocumentParser parser(in);

  std::unique_ptr<Document> document = parser.parse_document();

  ASSERT_EQ(document->catalog->pages->count, 1);
  ASSERT_EQ(document->catalog->pages->kids.size(), 1);
  const auto *page =
      dynamic_cast<Page *>(document->catalog->pages->kids.front());
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->contents_reference.size(), 1);
  EXPECT_EQ(parser.read_decoded_stream(page->contents_reference.front()),
            "BT ET");
}

} // namespace

TEST(DocumentParser, mini_pdf_with_classic_xref_table) {
  check_mini_pdf(two_object_mini_pdf(true));
}

TEST(DocumentParser, mini_pdf_with_xref_stream) {
  const std::string pdf = two_object_mini_pdf(false);

  std::istringstream in(pdf);
  DocumentParser parser(in);
  parser.parse_document();

  // 4 objects, the cross-reference stream itself, and the free head
  EXPECT_EQ(parser.xref().table.size(), 6);
  EXPECT_TRUE(parser.xref().table.at(ObjectReference(5, 0)).is_used());

  check_mini_pdf(pdf);
}

namespace {

void check_fixture_parses(const std::string &short_path,
                          const std::string &password = "") {
  SCOPED_TRACE(short_path);

  const auto file =
      std::make_shared<DiskFile>(TestData::test_file_path(short_path));

  const auto in = file->stream();
  DocumentParser parser(*in);

  const std::unique_ptr<Document> document = parser.parse_document(password);

  const std::vector<Page *> pages = document->collect_pages();

  EXPECT_FALSE(pages.empty());

  for (const Page *page : pages) {
    for (const auto &content_reference : page->contents_reference) {
      EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
    }
  }
}

} // namespace

TEST(DocumentParser, classic_xref_fixture) {
  check_fixture_parses("odr-public/pdf/style-various-1.pdf");
}

// Encrypted fixtures decrypt and parse end to end: object streams, string and
// content-stream decryption all run through the standard security handler.
TEST(DocumentParser, encrypted_rc4_fixture) {
  // R 2 / RC4-40, owner-locked: opens with the empty user password.
  check_fixture_parses("odr-public/pdf/Casio_WVA-M650-7AJF.pdf");
}

TEST(DocumentParser, encrypted_aes256_fixture) {
  // R 6 / V 5 / AESV3; password from the private-fixture index.csv. The
  // submodule is not always present, so skip when the file is absent.
  const std::string path = "odr-private/pdf/encrypted_fontfile3_opentype.pdf";
  if (!std::filesystem::exists(TestData::test_file_path(path))) {
    GTEST_SKIP() << "private fixture not available";
  }
  check_fixture_parses(path, "sample-user-password");
}

// The render path re-opens an encrypted file with the authenticated decryptor
// (carried from the probe parse) instead of the password. Deriving it once and
// replaying it must decrypt the document just as the password does.
TEST(DocumentParser, reopen_with_decryptor) {
  const std::string short_path = "odr-public/pdf/Casio_WVA-M650-7AJF.pdf";
  const auto file =
      std::make_shared<DiskFile>(TestData::test_file_path(short_path));

  // Authenticate via the empty user password (owner-locked file) and keep the
  // decryptor.
  std::shared_ptr<const Decryptor> decryptor;
  {
    const auto in = file->stream();
    DocumentParser parser(*in);
    parser.probe_encryption("");
    ASSERT_TRUE(parser.encrypted());
    ASSERT_TRUE(parser.authenticated());
    decryptor = parser.decryptor();
    ASSERT_NE(decryptor, nullptr);
  }

  // Re-open with the decryptor only; content streams must still decrypt.
  {
    const auto in = file->stream();
    DocumentParser parser(*in);
    const std::unique_ptr<Document> document = parser.parse_document(decryptor);
    const std::vector<Page *> pages = document->collect_pages();
    EXPECT_FALSE(pages.empty());
    for (const Page *page : pages) {
      for (const auto &content_reference : page->contents_reference) {
        EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
      }
    }
  }
}

TEST(DocumentParser, inherited_page_attributes) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R 7 0 R] /Count 3 "
              "/MediaBox [0 0 400 500] /Resources << >> /Rotate 90 >>")
      .object("<< /Type /Pages /Parent 2 0 R /Kids [4 0 R 5 0 R] /Count 2 "
              "/Rotate 180 >>")
      .object("<< /Type /Page /Parent 3 0 R /MediaBox [0 0 200 300] "
              "/Contents 6 0 R >>")
      .object("<< /Type /Page /Parent 3 0 R /MediaBox null /Resources null "
              "/Rotate null /Contents 6 0 R >>")
      .stream_object("", "BT ET")
      .object("<< /Type /Page /Parent 2 0 R /Contents 6 0 R >>")
      .trailer("/Root 1 0 R");

  std::istringstream in(builder.build_classic());
  DocumentParser parser(in);
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  ASSERT_EQ(pages.size(), 3);

  // page 4: own MediaBox, Rotate inherited from inner Pages, CropBox ←
  // MediaBox default, Resources inherited from root
  const Page *page4 = pages[0];
  EXPECT_EQ(page4->media_box.as_array()[2].as_real(), 200.0);
  EXPECT_EQ(page4->media_box.as_array()[3].as_real(), 300.0);
  EXPECT_EQ(page4->crop_box.as_array()[2].as_real(),
            200.0); // CropBox ← MediaBox
  EXPECT_EQ(page4->rotate, 180);
  ASSERT_NE(page4->resources, nullptr);

  // page 5: explicit null entries count as absent (7.3.9), so everything
  // still inherits (MediaBox/Resources from root, Rotate from inner Pages)
  const Page *page5 = pages[1];
  EXPECT_EQ(page5->media_box.as_array()[2].as_real(), 400.0);
  EXPECT_EQ(page5->media_box.as_array()[3].as_real(), 500.0);
  EXPECT_EQ(page5->rotate, 180);
  ASSERT_NE(page5->resources, nullptr);

  // page 6: MediaBox/Rotate inherited from root only
  const Page *page6 = pages[2];
  EXPECT_EQ(page6->media_box.as_array()[2].as_real(), 400.0);
  EXPECT_EQ(page6->rotate, 90);
}

TEST(DocumentParser, missing_media_box_defaults_to_us_letter) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R");

  std::istringstream in(builder.build_classic());
  DocumentParser parser(in);
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  ASSERT_EQ(pages.size(), 1);
  const Page *page = pages[0];
  // no MediaBox anywhere → US Letter, and CropBox follows it
  EXPECT_EQ(page->media_box.as_array()[2].as_real(), 612.0);
  EXPECT_EQ(page->media_box.as_array()[3].as_real(), 792.0);
  EXPECT_EQ(page->crop_box.as_array()[2].as_real(), 612.0);
  EXPECT_EQ(page->rotate, 0);
  ASSERT_NE(page->resources, nullptr); // missing /Resources → empty dict
}
