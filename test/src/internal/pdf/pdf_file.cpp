#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/html/pdf_file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

std::shared_ptr<pdf::PdfFile> open_pdf(const std::string &bytes) {
  return std::make_shared<pdf::PdfFile>(std::make_shared<MemoryFile>(bytes));
}

std::string render_html(const std::string &bytes, const PdfTextMode mode) {
  const odr::PdfFile file(open_pdf(bytes));
  HtmlConfig config;
  config.pdf_text_mode = mode;
  const std::shared_ptr<Logger> logger = Logger::create_null();
  const HtmlService service =
      internal::html::create_pdf_service(file, "", config, logger);
  std::ostringstream out;
  service.write("document.html", out);
  return std::move(out).str();
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

/// A three-page mini-PDF whose first page carries four `/Link` annotations: a
/// `/URI` action, a direct `/Dest` array to page 2, a `/GoTo` action to a named
/// destination (`chap3` → page 3, via the catalog `/Dests`), and a `/URI`
/// action with a `javascript:` scheme (which must not become a live link).
std::string link_annotations_mini_pdf() {
  PdfFileBuilder builder;
  builder
      .object("<< /Type /Catalog /Pages 2 0 R "
              "/Dests << /chap3 [5 0 R /Fit] >> >>")
      .object("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Annots [6 0 R 7 0 R 8 0 R 9 0 R] >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>")
      .object("<< /Type /Annot /Subtype /Link /Rect [100 700 200 720] "
              "/A << /S /URI /URI (http://example.com/?a=1&b=2) >> >>")
      .object("<< /Type /Annot /Subtype /Link /Rect [100 600 200 620] "
              "/Dest [4 0 R /Fit] >>")
      .object("<< /Type /Annot /Subtype /Link /Rect [100 500 200 520] "
              "/A << /S /GoTo /D (chap3) >> >>")
      .object("<< /Type /Annot /Subtype /Link /Rect [100 400 200 420] "
              "/A << /S /URI /URI (javascript:alert\\(1\\)) >> >>");
  return builder.trailer("/Root 1 0 R").build_classic();
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

// `/Link` annotations render as `<a>` overlays: a `/URI` action → external href
// (with `&` attr-escaped), a direct `/Dest` and a named `/GoTo` → internal
// `#pN` anchors that carry `target="_self"` (overriding `<base
// target="_blank">`); each page div carries a matching `id`. An active-scheme
// `/URI` is dropped.
TEST(PdfFile, link_annotations_render_as_anchors) {
  const std::string pdf = link_annotations_mini_pdf();
  for (const PdfTextMode mode :
       {PdfTextMode::dual_layer, PdfTextMode::single_layer}) {
    const std::string html = render_html(pdf, mode);
    EXPECT_TRUE(contains(html, R"(id="p1")"))
        << "mode " << static_cast<int>(mode);
    EXPECT_TRUE(contains(html, R"(id="p3")"))
        << "mode " << static_cast<int>(mode);
    EXPECT_TRUE(contains(html, R"(href="http://example.com/?a=1&amp;b=2")"))
        << "mode " << static_cast<int>(mode);
    EXPECT_TRUE(contains(html, R"(href="#p2" target="_self")"))
        << "mode " << static_cast<int>(mode);
    EXPECT_TRUE(contains(html, R"(href="#p3" target="_self")"))
        << "mode " << static_cast<int>(mode);
    // The `javascript:` action is not emitted as a link.
    EXPECT_FALSE(contains(html, "javascript:alert"))
        << "mode " << static_cast<int>(mode);
  }
}
