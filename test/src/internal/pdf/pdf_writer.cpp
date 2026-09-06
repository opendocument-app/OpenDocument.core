#include <odr/internal/pdf/pdf_writer.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <test_util.hpp>

#include <internal/pdf/pdf_test_file_builder.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

namespace {

std::string mini_pdf(const bool classic) {
  PdfFileBuilder builder;
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R /ID [<0102> <0304>]");
  return classic ? builder.build_classic() : builder.build_xref_stream();
}

std::unique_ptr<std::istringstream> stream_of(const std::string &pdf) {
  return std::make_unique<std::istringstream>(pdf);
}

template <typename SetUp>
std::string append(const std::string &pdf, SetUp &&set_up) {
  DocumentParser parser(stream_of(pdf));
  IncrementalWriter writer(parser);
  set_up(parser, writer);
  std::ostringstream out;
  writer.write(out);
  return std::move(out).str();
}

std::string append_nothing(const std::string &pdf) {
  return append(pdf, [](DocumentParser &, IncrementalWriter &) {});
}

const Page *first_page(const Document &document) {
  const auto &kids = document.catalog->pages->kids;
  return kids.empty() ? nullptr : dynamic_cast<Page *>(kids.front());
}

} // namespace

// An update that changes nothing still has to parse.
TEST(IncrementalWriter, no_op_update_preserves_the_document) {
  for (const bool classic : {true, false}) {
    SCOPED_TRACE(classic ? "classic" : "xref stream");

    const std::string source = mini_pdf(classic);
    const std::string result = append_nothing(source);

    ASSERT_GT(result.size(), source.size());
    EXPECT_EQ(result.substr(0, source.size()), source);

    DocumentParser parser(stream_of(result));
    EXPECT_FALSE(parser.is_recovered());
    EXPECT_EQ(parser.xref_kind(), classic ? DocumentParser::XrefKind::table
                                          : DocumentParser::XrefKind::stream);

    const std::unique_ptr<Document> document = parser.parse_document();
    ASSERT_EQ(document->catalog->pages->count, 1);
    const Page *page = first_page(*document);
    ASSERT_NE(page, nullptr);
    ASSERT_EQ(page->contents_reference.size(), 1);
    EXPECT_EQ(parser.read_decoded_stream(page->contents_reference.front()),
              "BT ET");
  }
}

// `/Prev` keeps the older objects reachable.
TEST(IncrementalWriter, trailer_chains_to_the_previous_section) {
  const std::string source = mini_pdf(true);
  const std::string result = append_nothing(source);

  const std::size_t previous = source.rfind("startxref\n");
  ASSERT_NE(previous, std::string::npos);
  const std::string previous_position = source.substr(
      previous + 10, source.find('\n', previous + 10) - previous - 10);

  EXPECT_NE(result.find("/Prev " + previous_position), std::string::npos);
  // `/ID[0]` carries over, `/ID[1]` names this revision
  EXPECT_NE(result.rfind("<0102>"), std::string::npos);
  EXPECT_EQ(result.rfind("<0304>"), source.rfind("<0304>"));
}

// Nothing in the writer reads a clock.
TEST(IncrementalWriter, output_is_deterministic) {
  const std::string source = mini_pdf(true);
  EXPECT_EQ(append_nothing(source), append_nothing(source));
}

// The newer definition wins; the older one stays, unreferenced.
TEST(IncrementalWriter, rewrites_page_rotate) {
  for (const bool classic : {true, false}) {
    SCOPED_TRACE(classic ? "classic" : "xref stream");

    const std::string result =
        append(mini_pdf(classic), [](DocumentParser &parser,
                                     IncrementalWriter &writer) {
          const std::unique_ptr<Document> document = parser.parse_document();
          const Page *page = first_page(*document);
          ASSERT_NE(page, nullptr);

          Dictionary rotated = page->object.as_dictionary();
          rotated["Rotate"] = Object(Integer{90});
          writer.set_object(page->object_reference, Object(std::move(rotated)));
        });

    DocumentParser parser(stream_of(result));
    const std::unique_ptr<Document> document = parser.parse_document();
    const Page *page = first_page(*document);
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page->rotate, 90);
    ASSERT_EQ(page->contents_reference.size(), 1);
    EXPECT_EQ(parser.read_decoded_stream(page->contents_reference.front()),
              "BT ET");
    EXPECT_EQ(page->media_box.as_array()[2].as_real(), 612.0);
  }
}

TEST(IncrementalWriter, appends_a_new_object) {
  const std::string result = append(
      mini_pdf(true), [](DocumentParser &parser, IncrementalWriter &writer) {
        const std::unique_ptr<Document> document = parser.parse_document();
        const Page *page = first_page(*document);
        ASSERT_NE(page, nullptr);

        // four objects in the mini pdf
        const ObjectReference annotation = writer.mint_object();
        EXPECT_EQ(annotation.id, 5u);

        Dictionary dictionary;
        dictionary["Type"] = Object(Name{"Annot"});
        dictionary["Subtype"] = Object(Name{"Square"});
        Array rect;
        rect.holder().emplace_back(Integer{10});
        rect.holder().emplace_back(Integer{20});
        rect.holder().emplace_back(Integer{30});
        rect.holder().emplace_back(Integer{40});
        dictionary["Rect"] = Object(std::move(rect));
        writer.set_object(annotation, Object(std::move(dictionary)));

        Dictionary annotated = page->object.as_dictionary();
        Array annotations;
        annotations.holder().emplace_back(annotation);
        annotated["Annots"] = Object(std::move(annotations));
        writer.set_object(page->object_reference, Object(std::move(annotated)));
      });

  DocumentParser parser(stream_of(result));
  const std::unique_ptr<Document> document = parser.parse_document();
  const Page *page = first_page(*document);
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->annotations.size(), 1);
  EXPECT_EQ(page->annotations.front()
                ->object.as_dictionary()
                .get("Subtype")
                .as_string(),
            "Square");
}

TEST(IncrementalWriter, appends_a_stream_object) {
  const std::string content = "0 0 10 10 re f";

  ObjectReference written;
  const std::string result =
      append(mini_pdf(true), [&written, &content](DocumentParser &,
                                                  IncrementalWriter &writer) {
        written = writer.mint_object();
        Dictionary dictionary;
        dictionary["Type"] = Object(Name{"XObject"});
        dictionary["Subtype"] = Object(Name{"Form"});
        writer.set_stream_object(written, std::move(dictionary), content);
      });

  DocumentParser parser(stream_of(result));
  EXPECT_EQ(parser.read_decoded_stream(written), content);
  const Object &dictionary = parser.read_object(written).object;
  EXPECT_EQ(dictionary.as_dictionary().get("Length").as_integer(),
            static_cast<Integer>(content.size()));
}

// A rebuilt table has no section of the file's own to chain onto.
TEST(IncrementalWriter, refuses_a_recovered_file) {
  const std::string pdf =
      "HTTP/1.0 200 OK\r\nContent-Type: application/pdf\r\n\r\n" +
      mini_pdf(true);
  DocumentParser parser(stream_of(pdf));
  ASSERT_TRUE(parser.is_recovered());
  EXPECT_ANY_THROW((void)IncrementalWriter(parser));
}

TEST(IncrementalWriter, refuses_an_encrypted_file) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/Casio_WVA-M650-7AJF.pdf"));
  DocumentParser parser(file->stream());
  ASSERT_TRUE(parser.is_encrypted());
  EXPECT_ANY_THROW((void)IncrementalWriter(parser));
}

TEST(IncrementalWriter, rewrites_a_page_of_a_real_fixture) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  std::ostringstream out;
  {
    DocumentParser parser(file->stream());
    const std::unique_ptr<Document> document = parser.parse_document();
    const Page *page = first_page(*document);
    ASSERT_NE(page, nullptr);

    IncrementalWriter writer(parser);
    Dictionary rotated = page->object.as_dictionary();
    rotated["Rotate"] = Object(Integer{270});
    writer.set_object(page->object_reference, Object(std::move(rotated)));
    writer.write(out);
  }

  DocumentParser parser(
      std::make_unique<std::istringstream>(std::move(out).str()));
  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();
  ASSERT_EQ(pages.size(), 2);
  EXPECT_EQ(pages[0]->rotate, 270);
  EXPECT_EQ(pages[1]->rotate, 0);

  EXPECT_FALSE(pages[0]->annotations.empty());
  for (const Page *page : pages) {
    for (const auto &content_reference : page->contents_reference) {
      EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
    }
  }
}

// A page dictionary compressed into an object stream is rewritten uncompressed
// in the new section; the newer type-1 entry wins over the older type-2 one.
TEST(IncrementalWriter, rewrites_a_page_out_of_an_object_stream) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/opendocument-app-website.pdf"));

  std::ostringstream out;
  ObjectReference page_reference;
  {
    DocumentParser parser(file->stream());
    ASSERT_EQ(parser.xref_kind(), DocumentParser::XrefKind::stream);

    const std::unique_ptr<Document> document = parser.parse_document();
    const Page *page = first_page(*document);
    ASSERT_NE(page, nullptr);
    page_reference = page->object_reference;
    // the fixture keeps its page objects in object streams
    ASSERT_TRUE(parser.xref().table.at(page_reference).is_compressed());

    IncrementalWriter writer(parser);
    Dictionary rotated = page->object.as_dictionary();
    rotated["Rotate"] = Object(Integer{90});
    writer.set_object(page_reference, Object(std::move(rotated)));
    writer.write(out);
  }

  DocumentParser parser(
      std::make_unique<std::istringstream>(std::move(out).str()));
  EXPECT_TRUE(parser.xref().table.at(page_reference).is_used());

  const std::unique_ptr<Document> document = parser.parse_document();
  const std::vector<Page *> pages = document->collect_pages();
  ASSERT_EQ(pages.size(), 9);
  EXPECT_EQ(pages[0]->rotate, 90);
  EXPECT_EQ(pages[1]->rotate, 0);
  for (const Page *page : pages) {
    for (const auto &content_reference : page->contents_reference) {
      EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
    }
  }
}
