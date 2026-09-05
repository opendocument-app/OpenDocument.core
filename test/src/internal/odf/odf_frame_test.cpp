#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

std::string write_odg(const std::string &name, const std::string &body) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0">)"
      R"(<office:body><office:drawing><draw:page>)" +
      body +
      R"(</draw:page></office:drawing></office:body>)"
      R"(</office:document-content>)";

  const std::string path = (std::filesystem::current_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.graphics"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  std::ofstream out(path);
  zip.save(out);

  return path;
}

std::vector<Frame> shapes_of(const Document &document) {
  std::vector<Frame> result;
  for (const Element child : document.root_element().first_child().children()) {
    result.push_back(child.as_frame());
  }
  return result;
}

} // namespace

TEST(OdfFrame, every_draw_shape_is_a_frame_naming_its_kind) {
  const std::string path = write_odg(
      "odf_shape_kinds.odg",
      R"(<draw:frame svg:x="1cm" svg:y="2cm"/>)"
      R"(<draw:g/>)"
      R"(<draw:rect svg:x="1cm" svg:y="2cm" svg:width="3cm" svg:height="4cm"/>)"
      R"(<draw:caption svg:x="1cm"/>)"
      R"(<draw:ellipse svg:x="1cm"/>)"
      R"(<draw:circle svg:x="1cm"/>)"
      R"(<draw:circle draw:kind="cut" svg:x="1cm"/>)"
      R"(<draw:line svg:x1="1cm" svg:y1="2cm" svg:x2="3cm" svg:y2="4cm"/>)"
      R"(<draw:measure svg:x1="1cm"/>)"
      R"(<draw:custom-shape svg:x="1cm"/>)"
      R"(<draw:polygon svg:x="1cm"/>)"
      R"(<draw:connector/>)");

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<Frame> shapes = shapes_of(document);

  std::vector<ShapeType> kinds;
  for (const Frame &shape : shapes) {
    EXPECT_EQ(shape.type(), ElementType::frame);
    kinds.push_back(shape.shape_type());
  }

  EXPECT_EQ(kinds,
            (std::vector<ShapeType>{
                ShapeType::none, ShapeType::none, ShapeType::rect,
                ShapeType::rect, ShapeType::ellipse, ShapeType::ellipse,
                ShapeType::custom, ShapeType::line, ShapeType::line,
                ShapeType::custom, ShapeType::custom, ShapeType::custom}));
}

TEST(OdfFrame, a_rect_carries_its_box_and_the_anchor_a_frame_has) {
  const std::string path = write_odg(
      "odf_shape_rect.odg",
      R"(<draw:rect text:anchor-type="paragraph" draw:z-index="3" )"
      R"(svg:x="1cm" svg:y="2cm" svg:width="3cm" svg:height="4cm"/>)");

  const Document document = odr::open(path).as_document_file().document();
  const Frame rect = shapes_of(document).at(0);

  EXPECT_EQ(rect.shape_type(), ShapeType::rect);
  EXPECT_EQ(rect.anchor_type(), AnchorType::at_paragraph);
  EXPECT_EQ(rect.z_index(), 3);
  ASSERT_TRUE(rect.x().has_value());
  EXPECT_EQ(rect.x()->to_string(), "1cm");
  EXPECT_EQ(rect.width()->to_string(), "3cm");
  EXPECT_FALSE(rect.line().has_value());
  EXPECT_FALSE(rect.path().has_value());
}

TEST(OdfFrame, a_line_states_its_two_ends) {
  const std::string path = write_odg(
      "odf_shape_line.odg",
      R"(<draw:line svg:x1="1cm" svg:y1="2cm" svg:x2="3cm" svg:y2="4cm"/>)");

  const Document document = odr::open(path).as_document_file().document();
  const Frame line = shapes_of(document).at(0);

  ASSERT_TRUE(line.line().has_value());
  EXPECT_EQ(line.line()->x1.to_string(), "1cm");
  EXPECT_EQ(line.line()->y1.to_string(), "2cm");
  EXPECT_EQ(line.line()->x2.to_string(), "3cm");
  EXPECT_EQ(line.line()->y2.to_string(), "4cm");
}

TEST(OdfFrame, a_custom_shape_carries_its_outline) {
  const std::string path =
      write_odg("odf_shape_custom.odg",
                R"(<draw:custom-shape svg:x="0cm" svg:y="0cm" )"
                R"(svg:width="2cm" svg:height="2cm">)"
                R"(<draw:enhanced-geometry svg:viewBox="0 0 100 100" )"
                R"(draw:enhanced-path="M 0 0 L 100 100 Z N"/>)"
                R"(</draw:custom-shape>)");

  const Document document = odr::open(path).as_document_file().document();
  const Frame shape = shapes_of(document).at(0);

  EXPECT_EQ(shape.shape_type(), ShapeType::custom);
  ASSERT_TRUE(shape.path().has_value());
  EXPECT_EQ(shape.path()->width, 100);
  EXPECT_FALSE(shape.path()->data.empty());
}
