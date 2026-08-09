#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;

namespace {

struct Marker final {
  std::string text;
  std::optional<std::uint32_t> number;
  ListType type{ListType::unordered};
};

void collect_markers(const Element element, const ListType type,
                     std::vector<Marker> &result) {
  for (const Element child : element.children()) {
    if (child.type() == ElementType::list) {
      collect_markers(child, child.as_list().type(), result);
      continue;
    }
    if (child.type() == ElementType::list_item) {
      const ListItem list_item = child.as_list_item();
      result.push_back({list_item.marker(), list_item.number(), type});
    }
    collect_markers(child, type, result);
  }
}

std::vector<Marker> markers_of(const std::string &short_path) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::warning);
  const DocumentFile document_file(TestData::test_file_path(short_path),
                                   logger);

  std::vector<Marker> result;
  collect_markers(document_file.document().root_element(), ListType::unordered,
                  result);
  return result;
}

std::vector<std::string> texts_of(const std::vector<Marker> &markers) {
  std::vector<std::string> result;
  result.reserve(markers.size());
  for (const Marker &marker : markers) {
    result.push_back(marker.text);
  }
  return result;
}

bool contains(const std::vector<std::string> &markers,
              const std::string &text) {
  return std::ranges::find(markers, text) != std::end(markers);
}

} // namespace

TEST(DocumentList, odt_resolves_bullets_and_numbers) {
  const std::vector<Marker> markers =
      markers_of("odr-public/odt/style-various-1.odt");

  EXPECT_EQ((std::vector<std::string>{"•", "•", "◦", "1.", "2.", "1."}),
            texts_of(markers));

  EXPECT_EQ(ListType::unordered, markers[0].type);
  EXPECT_FALSE(markers[0].number.has_value());
  EXPECT_EQ(ListType::ordered, markers[3].type);
  EXPECT_EQ(1, markers[3].number);
}

TEST(DocumentList, docx_resolves_the_same_document_the_same_way) {
  EXPECT_EQ((std::vector<std::string>{"•", "•", "◦", "1.", "2.", "1."}),
            texts_of(markers_of("odr-public/docx/style-various-1.docx")));
}

TEST(DocumentList, docx_resolves_multi_level_labels) {
  const std::vector<std::string> markers =
      texts_of(markers_of("odr-public/docx/sample1.docx"));

  // A `w:lvlText` of "%1.%2.%3." spells the whole path out at the deep level.
  EXPECT_TRUE(contains(markers, "1.1.1."));
  // Roman numerals, and a list that resumes after an interruption.
  EXPECT_TRUE(contains(markers, "iii."));
}

TEST(DocumentList, docx_keeps_counting_across_an_interleaved_list) {
  // `sample3.docx` breaks a numbered list with bullets and then goes on: Word
  // counts per `w:numId`, so the numbering does not restart at 1.
  const std::vector<Marker> markers =
      markers_of("odr-public/docx/sample3.docx");

  EXPECT_EQ(
      (std::vector<std::string>{"1.", "2.", "3.", "4.", "5.", "•", "•", "6."}),
      texts_of(markers));

  EXPECT_EQ(6, markers.back().number);
  EXPECT_FALSE(markers[5].number.has_value());
}
