#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>

#include <test_util.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <cstring>
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

const Page *first_page(const Document &document) {
  return dynamic_cast<Page *>(document.catalog->pages->kids.front());
}

const Font *first_page_font(const Document &document, const std::string &name) {
  const auto *page = first_page(document);
  return page->resources->font.at(name);
}

} // namespace

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
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));

  const std::unique_ptr<Document> document = parser.parse_document();

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

  const DocumentParser parser(std::make_unique<std::istringstream>(pdf));

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

  DocumentParser parser(file->stream());
  if (parser.authenticator().has_value()) {
    parser.authenticate(password);
  }

  const std::unique_ptr<Document> document = parser.parse_document();

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
// (carried from the initial open) instead of the password. Deriving it once and
// replaying it must decrypt the document just as the password does.
TEST(DocumentParser, reopen_with_decryptor) {
  const std::string short_path = "odr-public/pdf/Casio_WVA-M650-7AJF.pdf";
  const auto file =
      std::make_shared<DiskFile>(TestData::test_file_path(short_path));

  // Authenticate via the empty user password (owner-locked file) and keep the
  // decryptor.
  std::optional<Decryptor> decryptor;
  {
    const DocumentParser parser(file->stream());
    decryptor = parser.authenticator().value().authenticate("").value();
  }

  // Re-open with the decryptor only; content streams must still decrypt.
  {
    DocumentParser parser(file->stream(), decryptor);
    const std::unique_ptr<Document> document = parser.parse_document();
    const std::vector<Page *> pages = document->collect_pages();
    EXPECT_FALSE(pages.empty());
    for (const Page *page : pages) {
      for (const auto &content_reference : page->contents_reference) {
        EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
      }
    }
  }
}

// Reading an encrypted file without authenticating must throw rather than serve
// undecrypted bytes. The file reports as encrypted but not authenticated.
TEST(DocumentParser, read_without_authentication_throws) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/Casio_WVA-M650-7AJF.pdf"));

  DocumentParser parser(file->stream());
  EXPECT_TRUE(parser.is_encrypted());
  EXPECT_FALSE(parser.is_authenticated());

  EXPECT_THROW((void)parser.parse_document(), odr::UnauthenticatedReadError);
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

  DocumentParser parser(
      std::make_unique<std::istringstream>(builder.build_classic()));
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

namespace {

/// A mini-PDF whose single page references one composite (Type0) font `F0`:
/// `encoding` (a predefined CMap name) over a descendant `CIDFontType2` with an
/// `Adobe`/`Identity` `/CIDSystemInfo`, optionally carrying a 2-byte
/// `/ToUnicode` CMap (mapping the code 0x0041 to `A`).
std::string
composite_font_mini_pdf(const bool with_to_unicode,
                        const std::string &encoding = "Identity-H") {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /Font << /F0 5 0 R >> >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .object(std::string("<< /Type /Font /Subtype /Type0 /BaseFont /AAAAAA+X "
                          "/Encoding /") +
              encoding + " /DescendantFonts [6 0 R]" +
              (with_to_unicode ? " /ToUnicode 7 0 R >>" : " >>"))
      .object("<< /Type /Font /Subtype /CIDFontType2 /BaseFont /AAAAAA+X "
              "/CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) "
              "/Supplement 0 >> /CIDToGIDMap /Identity "
              "/DW 1000 /W [0 [500 600]] >>");
  if (with_to_unicode) {
    builder.stream_object("", "1 begincodespacerange\n<0000> <FFFF>\n"
                              "endcodespacerange\n1 beginbfchar\n"
                              "<0041> <0041>\nendbfchar\n");
  }
  return builder.trailer("/Root 1 0 R").build_classic();
}

/// A mini-PDF whose single page references one simple TrueType font `F1` with
/// `/FirstChar` 65, `/Widths` for `A`/`B`, and a `/FontDescriptor` carrying
/// `/MissingWidth`.
std::string simple_font_mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      // `/Encoding` remaps code 67 to a glyph absent from the AFM table, so its
      // advance can only come from `/MissingWidth` — not the substitute's
      // built-in code->width table, which the explicit encoding overrides.
      .object("<< /Type /Font /Subtype /TrueType /BaseFont /Helvetica "
              "/FirstChar 65 /LastChar 66 /Widths [500 600] "
              "/Encoding << /Differences [67 /customglyph] >> "
              "/FontDescriptor 6 0 R >>")
      .object("<< /Type /FontDescriptor /FontName /Helvetica "
              "/MissingWidth 250 >>");
  return builder.trailer("/Root 1 0 R").build_classic();
}

/// A mini-PDF whose page lists a form XObject `Fm0` (with a `/Matrix`). `Fm0`
/// and `Fm1` reference each other through their `/Resources`, forming a cycle
/// (Fm0 -> Fm1 -> Fm0).
std::string form_xobject_cycle_mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /XObject << /Fm0 5 0 R >> >> /Contents 4 0 R >>")
      .stream_object("", "/Fm0 Do")
      .stream_object("/Type /XObject /Subtype /Form /BBox [0 0 100 100] "
                     "/Matrix [1 0 0 1 10 20] "
                     "/Resources << /XObject << /Fm1 6 0 R >> >>",
                     "/Fm1 Do")
      .stream_object("/Type /XObject /Subtype /Form /BBox [0 0 100 100] "
                     "/Resources << /XObject << /Fm0 5 0 R >> >>",
                     "/Fm0 Do");
  return builder.trailer("/Root 1 0 R").build_classic();
}

/// A mini-PDF with two pages whose resources both reference the same form
/// XObject (object 6).
std::string shared_form_xobject_mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /XObject << /Fm0 6 0 R >> >> /Contents 5 0 R >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /XObject << /Fm0 6 0 R >> >> /Contents 5 0 R >>")
      .stream_object("", "/Fm0 Do")
      .stream_object("/Type /XObject /Subtype /Form /BBox [0 0 100 100]", "");
  return builder.trailer("/Root 1 0 R").build_classic();
}

/// A mini-PDF whose page defines a named colour space `/CS0` in its
/// `/Resources /ColorSpace` and an image XObject `Im0` referencing it by name.
/// The image is a 2x2 RGB raster carried as ASCIIHex so the source stays text.
std::string named_colorspace_image_mini_pdf() {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << /XObject << /Im0 5 0 R >> "
              "/ColorSpace << /CS0 [/CalRGB << /WhitePoint [1 1 1] >>] >> >> "
              "/Contents 4 0 R >>")
      .stream_object("", "/Im0 Do")
      .stream_object("/Type /XObject /Subtype /Image /Width 2 /Height 2 "
                     "/BitsPerComponent 8 /ColorSpace /CS0 "
                     "/Filter /ASCIIHexDecode",
                     "FF000000FF000000FFFFFFFF>");
  return builder.trailer("/Root 1 0 R").build_classic();
}

} // namespace

// A composite (Type0) font is recognized, its descendant CIDFont's
// `/CIDSystemInfo` recorded, and its `/ToUnicode` CMap drives extraction over
// 2-byte codes.
TEST(DocumentParser, composite_font_with_to_unicode) {
  const std::string pdf = composite_font_mini_pdf(true);
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Font *font = first_page_font(*document, "F0");
  ASSERT_NE(font, nullptr);
  EXPECT_TRUE(font->composite);
  EXPECT_EQ(font->cid_registry, "Adobe");
  EXPECT_EQ(font->cid_ordering, "Identity");
  // The 2-byte code 0x0041 maps to `A` via the `/ToUnicode` CMap.
  EXPECT_EQ(font->to_unicode(std::string("\x00\x41", 2)), "A");
}

// A composite font without a `/ToUnicode` CMap cannot yet resolve CID ->
// Unicode (predefined CJK tables and embedded reverse maps are deferred), so
// extraction yields "no Unicode" rather than the byte-garbage the simple-font
// identity fallback would produce on multi-byte codes.
TEST(DocumentParser, composite_font_without_to_unicode_yields_no_unicode) {
  const std::string pdf = composite_font_mini_pdf(false);
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Font *font = first_page_font(*document, "F0");
  ASSERT_NE(font, nullptr);
  EXPECT_TRUE(font->composite);
  EXPECT_EQ(font->cid_registry, "Adobe");
  EXPECT_TRUE(font->to_unicode(std::string("\x00\x41", 2)).empty());
}

// A composite font whose `/Encoding` is a predefined Unicode CMap
// (`Uni*-UCS2/UTF16/UTF32`) extracts directly from the codes (they are Unicode)
// even without a `/ToUnicode` CMap.
TEST(DocumentParser, composite_font_predefined_unicode_cmap) {
  const std::string pdf = composite_font_mini_pdf(false, "UniGB-UCS2-H");
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Font *font = first_page_font(*document, "F0");
  ASSERT_NE(font, nullptr);
  EXPECT_TRUE(font->composite);
  EXPECT_EQ(font->cid_encoding_name, "UniGB-UCS2-H");
  // The 2-byte codes are UTF-16BE: U+0041 'A', U+4E2D '中' (UTF-8 e4 b8 ad).
  EXPECT_EQ(font->to_unicode(std::string("\x00\x41\x4e\x2d", 4)),
            "A\xe4\xb8\xad");
}

// A composite font's `/W` array and `/DW` default drive CID advance widths.
TEST(DocumentParser, composite_font_cid_widths) {
  const std::string pdf = composite_font_mini_pdf(true);
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Font *font = first_page_font(*document, "F0");
  ASSERT_NE(font, nullptr);
  EXPECT_DOUBLE_EQ(font->cid_default_width, 1000);
  EXPECT_DOUBLE_EQ(font->advance_width(0), 0.5); // W [0 [500 600]]
  EXPECT_DOUBLE_EQ(font->advance_width(1), 0.6);
  EXPECT_DOUBLE_EQ(font->advance_width(2), 1.0); // /DW default
}

// Form XObjects are parsed onto the resources with their `/Matrix` and decoded
// content. A cyclic `/Resources` reference (Fm0 -> Fm1 -> Fm0) terminates via
// the parser's XObject cache and is represented faithfully: the back-edge
// points at the very same `Fm0` element (no duplicate, no clipping).
TEST(DocumentParser, form_xobject_cycle_is_represented_via_cache) {
  const std::string pdf = form_xobject_cycle_mini_pdf();
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Page *page = first_page(*document);
  ASSERT_NE(page->resources, nullptr);

  const XObject *fm0 = page->resources->x_object.at("Fm0");
  ASSERT_NE(fm0, nullptr);
  EXPECT_EQ(fm0->subtype, XObject::Subtype::form);
  EXPECT_EQ(fm0->content, "/Fm1 Do");
  EXPECT_DOUBLE_EQ(fm0->matrix.e, 10); // /Matrix [1 0 0 1 10 20]
  EXPECT_DOUBLE_EQ(fm0->matrix.f, 20);

  ASSERT_NE(fm0->resources, nullptr);
  const XObject *fm1 = fm0->resources->x_object.at("Fm1");
  ASSERT_NE(fm1, nullptr);
  EXPECT_EQ(fm1->subtype, XObject::Subtype::form);

  // Fm1 -> Fm0 closes the cycle: it resolves to the same cached element.
  ASSERT_NE(fm1->resources, nullptr);
  EXPECT_EQ(fm1->resources->x_object.at("Fm0"), fm0);
}

// An image XObject whose `/ColorSpace` is a name defined in the enclosing
// `/Resources /ColorSpace` table resolves — the table is parsed before the
// `/XObject` table and threaded into image parsing — and the raster is encoded
// to PNG, rather than being dropped for an unresolved colour space.
TEST(DocumentParser, image_named_colorspace_resolves_and_encodes) {
  const std::string pdf = named_colorspace_image_mini_pdf();
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Page *page = first_page(*document);
  ASSERT_NE(page->resources, nullptr);
  const XObject *image = page->resources->x_object.at("Im0");
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->subtype, XObject::Subtype::image);
  EXPECT_FALSE(image->image_data.empty());
  EXPECT_EQ(image->image_mime, "image/png");
}

// A form XObject shared by two pages is parsed once: both pages' resources
// point at the same element (the parser's XObject cache dedups by reference).
TEST(DocumentParser, shared_form_xobject_is_parsed_once) {
  const std::string pdf = shared_form_xobject_mini_pdf();
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const std::vector<Page *> pages = document->collect_pages();
  ASSERT_EQ(pages.size(), 2);
  ASSERT_NE(pages[0]->resources, nullptr);
  ASSERT_NE(pages[1]->resources, nullptr);

  const XObject *fm_a = pages[0]->resources->x_object.at("Fm0");
  const XObject *fm_b = pages[1]->resources->x_object.at("Fm0");
  ASSERT_NE(fm_a, nullptr);
  EXPECT_EQ(fm_a, fm_b);
}

// A simple font's `/FirstChar`, `/Widths` and `/MissingWidth` drive advances.
// Code 67 is outside `/Widths` and its `/Encoding` glyph is absent from the
// substitute's AFM table, so it falls back to `/MissingWidth`.
TEST(DocumentParser, simple_font_widths) {
  const std::string pdf = simple_font_mini_pdf();
  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();

  const Font *font = first_page_font(*document, "F1");
  ASSERT_NE(font, nullptr);
  EXPECT_FALSE(font->composite);
  EXPECT_EQ(font->first_char, 65);
  EXPECT_DOUBLE_EQ(font->advance_width(65), 0.5); // 'A'
  EXPECT_DOUBLE_EQ(font->advance_width(66), 0.6); // 'B'
  EXPECT_DOUBLE_EQ(font->advance_width(67),
                   0.25); // /customglyph -> /MissingWidth
}

// Recovery: a valid file with garbage prepended (the real fixture
// `order-EK52VKL0.pdf` is an HTTP response saved as `.pdf`) has every xref
// offset and the `startxref` shifted, so the chain walk fails. A forward scan
// rebuilds the table from the actual object positions.
TEST(DocumentParser, recovers_from_prepended_garbage) {
  const std::string pdf =
      "HTTP/1.0 200 OK\r\nContent-Type: application/pdf\r\n\r\n" +
      two_object_mini_pdf(true);
  check_mini_pdf(pdf);
}

// Recovery: the `startxref` points nowhere, so locating the table fails.
TEST(DocumentParser, recovers_from_garbage_startxref) {
  std::string pdf = two_object_mini_pdf(true);
  const std::size_t pos = pdf.find("startxref\n") + std::strlen("startxref\n");
  pdf.replace(pos, pdf.find('\n', pos) - pos, "999999");
  check_mini_pdf(pdf);
}

// Recovery: no `xref`/`trailer`/`startxref` at all — the catalog is found by
// scanning the recovered objects for `/Type /Catalog`.
TEST(DocumentParser, recovers_root_from_catalog_scan) {
  const std::string pdf =
      "%PDF-1.7\n"
      "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
      "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
      "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
      "/Resources << >> /Contents 4 0 R >>\nendobj\n"
      "4 0 obj\n<< /Length 5 >>\nstream\nBT ET\nendstream\nendobj\n"
      "%%EOF\n";
  check_mini_pdf(pdf);
}

// Recovery: an id defined more than once (e.g. a botched incremental update)
// resolves to the last definition in the file.
TEST(DocumentParser, recovery_last_definition_wins) {
  const std::string pdf =
      "%PDF-1.7\n"
      "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
      "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
      "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] "
      "/Resources << >> /Contents 4 0 R >>\nendobj\n"
      "4 0 obj\n<< /Length 5 >>\nstream\nBT ET\nendstream\nendobj\n"
      "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] "
      "/Resources << >> /Contents 4 0 R >>\nendobj\n"
      "trailer\n<< /Root 1 0 R >>\n%%EOF\n";

  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  ASSERT_EQ(pages.size(), 1);
  EXPECT_EQ(pages[0]->media_box.as_array()[2].as_real(), 200.0);
}

// Recovery: an object that inlines its dictionary and the `stream` token on a
// single line (`N G obj<<...>>stream`) must still have its body skipped, so
// object-shaped bytes inside the stream (here a fake `1 0 obj`) do not
// overwrite the real recovered entry.
TEST(DocumentParser, recovery_skips_same_line_stream_body) {
  const std::string pdf =
      "%PDF-1.7\n"
      "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
      "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
      "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
      "/Resources << >> /Contents 4 0 R >>\nendobj\n"
      // dictionary and `stream` keyword share the object's first line; the body
      // contains a decoy `1 0 obj` that must not be recorded
      "4 0 obj<< /Length 20 >>stream\n1 0 obj garbage BT "
      "ET\nendstream\nendobj\n"
      "trailer\n<< /Root 1 0 R >>\n%%EOF\n";

  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  ASSERT_EQ(pages.size(), 1);
  // the real catalog (object 1) survived; the decoy inside the stream did not
  // clobber it
  EXPECT_EQ(pages[0]->media_box.as_array()[2].as_real(), 612.0);
}

// Recovery: the page tree lives in an (uncompressed) object stream. After the
// forward scan finds the stream, its members are indexed as compressed entries
// so the catalog and pages resolve.
TEST(DocumentParser, recovers_object_stream_members) {
  const std::vector<std::pair<int, std::string>> members = {
      {2, "<< /Type /Catalog /Pages 3 0 R >>"},
      {3, "<< /Type /Pages /Kids [4 0 R] /Count 1 >>"},
      {4, "<< /Type /Page /Parent 3 0 R /MediaBox [0 0 612 792] "
          "/Resources << >> /Contents 6 0 R >>"}};

  std::string header;
  std::string payload;
  for (const auto &[id, body] : members) {
    header += std::to_string(id) + " " + std::to_string(payload.size()) + " ";
    payload += body + " ";
  }
  const std::string objstm = header + payload;

  std::string pdf = "%PDF-1.7\n";
  pdf += "5 0 obj\n<< /Type /ObjStm /N " + std::to_string(members.size()) +
         " /First " + std::to_string(header.size()) + " /Length " +
         std::to_string(objstm.size()) + " >>\nstream\n" + objstm +
         "\nendstream\nendobj\n";
  pdf += "6 0 obj\n<< /Length 5 >>\nstream\nBT ET\nendstream\nendobj\n";
  pdf += "%%EOF\n";

  DocumentParser parser(std::make_unique<std::istringstream>(pdf));
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();

  ASSERT_EQ(pages.size(), 1);
  EXPECT_EQ(pages[0]->media_box.as_array()[2].as_real(), 612.0);
  EXPECT_EQ(parser.read_decoded_stream(pages[0]->contents_reference.front()),
            "BT ET");
}

// Real-world recovery: an HTTP response accidentally saved as `.pdf` — the body
// is a valid PDF but the leading `HTTP/1.0 200 OK …` header shifts every
// offset. Skipped when the private submodule is absent.
TEST(DocumentParser, recovers_http_response_fixture) {
  const std::string path = "odr-private/pdf/order-EK52VKL0.pdf";
  if (!std::filesystem::exists(TestData::test_file_path(path))) {
    GTEST_SKIP() << "private fixture not available";
  }
  check_fixture_parses(path);
}

TEST(DocumentParser, missing_media_box_defaults_to_us_letter) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R");

  DocumentParser parser(
      std::make_unique<std::istringstream>(builder.build_classic()));
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
