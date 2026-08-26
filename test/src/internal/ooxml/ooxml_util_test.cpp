#include <odr/internal/ooxml/ooxml_util.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

using namespace odr::internal;
using namespace odr::internal::ooxml;

namespace {

constexpr const char *slide_path = "/ppt/slides/slide1.xml";
constexpr const char *rels_path = "/ppt/slides/_rels/slide1.xml.rels";
constexpr const char *layout_type =
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
    "slideLayout";

VirtualFilesystem filesystem_of(const std::string &relationships) {
  VirtualFilesystem result;
  result.copy(std::make_shared<MemoryFile>(std::string()), AbsPath(slide_path));
  if (!relationships.empty()) {
    result.copy(std::make_shared<MemoryFile>(relationships),
                AbsPath(rels_path));
  }
  return result;
}

std::string relationship(const std::string &type, const std::string &target) {
  return R"(<Relationship Id="rId1" Type=")" + type + R"(" Target=")" + target +
         R"("/>)";
}

std::string relationships_of(const std::string &children) {
  return R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)" +
         children + "</Relationships>";
}

std::optional<std::string> layout_of(const std::string &relationships) {
  const VirtualFilesystem filesystem = filesystem_of(relationships);
  const std::optional<AbsPath> result =
      parse_relationship_target(filesystem, AbsPath(slide_path), "slideLayout");
  if (!result.has_value()) {
    return {};
  }
  return result->string();
}

} // namespace

TEST(ooxml_util, relationship_target_resolves_against_the_part) {
  EXPECT_EQ(layout_of(relationships_of(
                relationship(layout_type, "../slideLayouts/slideLayout1.xml"))),
            "/ppt/slideLayouts/slideLayout1.xml");
}

TEST(ooxml_util, relationship_target_of_a_part_without_relationships) {
  EXPECT_FALSE(layout_of("").has_value());
}

TEST(ooxml_util, relationship_type_matches_a_whole_trailing_segment) {
  // ends with the type, but not on a `/` boundary
  EXPECT_FALSE(layout_of(relationships_of(relationship(
                             "http://example.com/relationships/notSlideLayout",
                             "layout.xml")))
                   .has_value());
  // the type is the whole uri, so there is no segment before it
  EXPECT_FALSE(
      layout_of(relationships_of(relationship("slideLayout", "layout.xml")))
          .has_value());
  EXPECT_EQ(layout_of(relationships_of(
                relationship("http://example.com/rel/slideLayout", "l.xml"))),
            "/ppt/slides/l.xml");
}

TEST(ooxml_util, relationship_target_takes_the_first_of_its_type) {
  EXPECT_EQ(layout_of(relationships_of(
                relationship("http://example.com/rel/theme", "theme.xml") +
                relationship(layout_type, "first.xml") +
                relationship(layout_type, "second.xml"))),
            "/ppt/slides/first.xml");
}

TEST(ooxml_util, relationship_target_of_a_type_nothing_relates) {
  EXPECT_FALSE(layout_of(relationships_of(relationship(
                             "http://example.com/rel/theme", "theme.xml")))
                   .has_value());
}

TEST(ooxml_util, an_absolute_relationship_target_names_a_part_from_the_root) {
  EXPECT_EQ(layout_of(relationships_of(relationship(
                layout_type, "/ppt/slideLayouts/slideLayout1.xml"))),
            "/ppt/slideLayouts/slideLayout1.xml");
}

TEST(ooxml_util, a_relationship_target_that_names_nothing_resolves_to_nothing) {
  EXPECT_FALSE(
      layout_of(relationships_of(relationship(layout_type, ""))).has_value());
  // climbing out of the package
  EXPECT_FALSE(
      layout_of(relationships_of(relationship(layout_type, "../../../x.xml")))
          .has_value());
}
