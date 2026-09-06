#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>

#include <test_util.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::test;
using odr::internal::pdf::DocumentParser;
using odr::internal::pdf::Page;
using PdfDocument = odr::internal::pdf::Document;

namespace {

PdfFile open_fixture(const std::string &short_path) {
  return open(File(TestData::test_file_path(short_path)), DecodeOptions{},
              Logger::null())
      .as_pdf_file();
}

std::string annotate(const std::string &json) {
  std::ostringstream out;
  open_fixture("odr-public/pdf/style-various-1.pdf").annotate(json, out);
  return std::move(out).str();
}

std::vector<Page *> pages_of(DocumentParser &parser,
                             std::unique_ptr<PdfDocument> &document) {
  document = parser.parse_document();
  return document->collect_pages();
}

constexpr std::string_view one_highlight = R"json({
  "version": 1,
  "annotations": [
    {
      "page": 0,
      "type": "highlight",
      "quads": [[72, 700, 300, 700, 72, 688, 300, 688]],
      "color": [1, 0.9, 0.2]
    }
  ]
})json";

} // namespace

TEST(PdfAnnotate, capability_is_declared) {
  EXPECT_TRUE(
      capabilities_by_file_type(FileType::portable_document_format).annotate);
}

TEST(PdfAnnotate, writes_a_highlight) {
  const std::string result = annotate(std::string(one_highlight));

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  std::unique_ptr<PdfDocument> document;
  const std::vector<Page *> pages = pages_of(parser, document);
  ASSERT_EQ(pages.size(), 2);

  // the fixture's own link annotations plus ours
  const auto &annotations = pages[0]->annotations;
  ASSERT_FALSE(annotations.empty());
  const auto &dictionary = annotations.back()->object.as_dictionary();
  EXPECT_EQ(dictionary.get("Subtype").as_string(), "Highlight");
  EXPECT_NE(annotations.back()->appearance, nullptr);
}

TEST(PdfAnnotate, writes_every_type) {
  const std::string result = annotate(R"json({
    "version": 1,
    "annotations": [
      {"page": 0, "type": "highlight",
       "quads": [[72, 700, 300, 700, 72, 688, 300, 688]], "color": [1, 1, 0]},
      {"page": 0, "type": "underline",
       "quads": [[72, 660, 300, 660, 72, 648, 300, 648]], "color": [0, 0, 1]},
      {"page": 0, "type": "strikeOut",
       "quads": [[72, 630, 300, 630, 72, 618, 300, 618]], "color": [1, 0, 0]},
      {"page": 1, "type": "squiggly",
       "quads": [[72, 600, 300, 600, 72, 588, 300, 588]], "color": [0, 1, 0]},
      {"page": 1, "type": "ink", "strokes": [[100, 500, 130, 540, 160, 490]],
       "width": 2, "color": [0, 0, 0]}
    ]
  })json");

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  std::unique_ptr<PdfDocument> document;
  const std::vector<Page *> pages = pages_of(parser, document);
  ASSERT_EQ(pages.size(), 2);

  const auto subtypes = [](const Page &page) {
    std::vector<std::string> result;
    for (const auto *annotation : page.annotations) {
      result.push_back(
          annotation->object.as_dictionary().get("Subtype").as_string());
    }
    return result;
  };

  const std::vector<std::string> first = subtypes(*pages[0]);
  EXPECT_NE(std::ranges::find(first, "Highlight"), first.end());
  EXPECT_NE(std::ranges::find(first, "Underline"), first.end());
  EXPECT_NE(std::ranges::find(first, "StrikeOut"), first.end());

  const std::vector<std::string> second = subtypes(*pages[1]);
  EXPECT_NE(std::ranges::find(second, "Squiggly"), second.end());
  EXPECT_NE(std::ranges::find(second, "Ink"), second.end());
}

TEST(PdfAnnotate, author_and_contents_reach_the_file) {
  const std::string result = annotate(R"json({
    "version": 1,
    "annotations": [
      {"page": 0, "type": "highlight",
       "quads": [[72, 700, 300, 700, 72, 688, 300, 688]],
       "color": [1, 1, 0], "opacity": 0.5,
       "author": "a reviewer", "contents": "look (here)"}
    ]
  })json");

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  std::unique_ptr<PdfDocument> document;
  const std::vector<Page *> pages = pages_of(parser, document);
  const auto &dictionary = pages[0]->annotations.back()->object.as_dictionary();
  EXPECT_EQ(dictionary.get("T").as_string(), "a reviewer");
  EXPECT_EQ(dictionary.get("Contents").as_string(), "look (here)");
  EXPECT_DOUBLE_EQ(dictionary.get("CA").as_real(), 0.5);
}

// An empty payload is legal and writes an update that changes nothing.
TEST(PdfAnnotate, empty_payload_is_a_no_op_update) {
  const std::string result =
      annotate(R"json({"version": 1, "annotations": []})json");

  DocumentParser parser(std::make_unique<std::istringstream>(result));
  std::unique_ptr<PdfDocument> document;
  const std::vector<Page *> pages = pages_of(parser, document);
  EXPECT_EQ(pages.size(), 2);
}

TEST(PdfAnnotate, malformed_payloads_are_refused) {
  EXPECT_THROW(annotate("not json"), std::invalid_argument);
  EXPECT_THROW(annotate(R"json({"annotations": []})json"),
               std::invalid_argument);
  EXPECT_THROW(annotate(R"json({"version": 2, "annotations": []})json"),
               std::invalid_argument);
  // a type we do not write
  EXPECT_THROW(annotate(R"json({"version": 1, "annotations":
      [{"page": 0, "type": "stamp", "color": [0, 0, 0]}]})json"),
               std::invalid_argument);
  // a page that is not there
  EXPECT_THROW(annotate(R"json({"version": 1, "annotations":
      [{"page": 99, "type": "highlight", "quads": [[0,0,0,0,0,0,0,0]],
        "color": [0, 0, 0]}]})json"),
               std::invalid_argument);
  // a quad that is not eight coordinates
  EXPECT_THROW(annotate(R"json({"version": 1, "annotations":
      [{"page": 0, "type": "highlight", "quads": [[0, 0]],
        "color": [0, 0, 0]}]})json"),
               std::invalid_argument);
  // a required member missing
  EXPECT_THROW(annotate(R"json({"version": 1, "annotations":
      [{"page": 0, "type": "highlight",
        "quads": [[0,0,0,0,0,0,0,0]]}]})json"),
               std::invalid_argument);
}

// Decision 6: an encrypted file cannot take an incremental update, because the
// key its new objects would need is not retained.
TEST(PdfAnnotate, refuses_an_encrypted_file) {
  std::ostringstream out;
  EXPECT_ANY_THROW(open_fixture("odr-public/pdf/Casio_WVA-M650-7AJF.pdf")
                       .annotate(std::string(one_highlight), out));
}
