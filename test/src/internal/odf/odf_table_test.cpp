#include <odr/internal/odf/odf_table.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <pugixml.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::odf;

namespace {

std::vector<pugi::xml_node> rows_of(const pugi::xml_node table) {
  std::vector<pugi::xml_node> result;
  for_each_table_row(table,
                     [&](const pugi::xml_node row) { result.push_back(row); });
  return result;
}

std::vector<pugi::xml_node> columns_of(const pugi::xml_node table) {
  std::vector<pugi::xml_node> result;
  for_each_table_column(
      table, [&](const pugi::xml_node column) { result.push_back(column); });
  return result;
}

std::vector<std::string> names_of(const std::vector<pugi::xml_node> &nodes) {
  std::vector<std::string> result;
  for (const pugi::xml_node node : nodes) {
    result.emplace_back(node.attribute("id").value());
  }
  return result;
}

pugi::xml_node parse_table(pugi::xml_document &document,
                           const std::string &xml) {
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return document.child("table:table");
}

/// An odf package is a zip with a mimetype and a `content.xml`, which keeps
/// the input to these tests a string.
std::string write_odf(const std::string &name, const std::string &mimetype,
                      const std::string &body) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0">)"
      R"(<office:body>)" +
      body + R"(</office:body></office:document-content>)";

  const std::string path = (std::filesystem::current_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(mimetype));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  std::ofstream out(path);
  zip.save(out);

  return path;
}

void collect_text(const Element element, std::vector<std::string> &out) {
  if (!element) {
    return;
  }
  if (const Text text = element.as_text()) {
    out.push_back(text.content());
  }
  for (const Element child : element.children()) {
    collect_text(child, out);
  }
}

/// An `Element` points into the document, which has to outlive the walk.
std::vector<std::string> text_of(const Document &document) {
  std::vector<std::string> result;
  collect_text(document.root_element(), result);
  return result;
}

constexpr auto header_row_table = R"(<table:table table:name="T1">
  <table:table-column table:number-columns-repeated="2" id="c1"/>
  <table:table-header-rows>
    <table:table-row id="header">
      <table:table-cell office:value-type="string"><text:p>HEADER_A</text:p></table:table-cell>
      <table:table-cell office:value-type="string"><text:p>HEADER_B</text:p></table:table-cell>
    </table:table-row>
  </table:table-header-rows>
  <table:table-row id="body">
    <table:table-cell office:value-type="string"><text:p>BODY_A</text:p></table:table-cell>
    <table:table-cell office:value-type="string"><text:p>BODY_B</text:p></table:table-cell>
  </table:table-row>
</table:table>)";

} // namespace

TEST(OdfTable, rows_directly_under_the_table) {
  pugi::xml_document document;
  const pugi::xml_node table = parse_table(document, R"(
      <table:table>
        <table:table-row id="a"/>
        <table:table-row id="b"/>
      </table:table>)");

  EXPECT_EQ(names_of(rows_of(table)), (std::vector<std::string>{"a", "b"}));
}

TEST(OdfTable, rows_inside_a_grouping_element) {
  pugi::xml_document document;
  const pugi::xml_node table = parse_table(document, R"(
      <table:table>
        <table:table-header-rows>
          <table:table-row id="header"/>
        </table:table-header-rows>
        <table:table-rows>
          <table:table-row id="body"/>
        </table:table-rows>
      </table:table>)");

  EXPECT_EQ(names_of(rows_of(table)),
            (std::vector<std::string>{"header", "body"}));
}

TEST(OdfTable, row_groups_nest) {
  pugi::xml_document document;
  const pugi::xml_node table = parse_table(document, R"(
      <table:table>
        <table:table-row-group>
          <table:table-row id="a"/>
          <table:table-row-group>
            <table:table-header-rows>
              <table:table-row id="b"/>
            </table:table-header-rows>
          </table:table-row-group>
        </table:table-row-group>
        <table:table-row id="c"/>
      </table:table>)");

  EXPECT_EQ(names_of(rows_of(table)),
            (std::vector<std::string>{"a", "b", "c"}));
}

TEST(OdfTable, a_collapsed_group_is_not_shown) {
  pugi::xml_document document;
  const pugi::xml_node table = parse_table(document, R"(
      <table:table>
        <table:table-row-group table:display="false">
          <table:table-row id="hidden"/>
          <table:table-row-group>
            <table:table-row id="hidden-too"/>
          </table:table-row-group>
        </table:table-row-group>
        <table:table-row-group table:display="true">
          <table:table-row id="shown"/>
        </table:table-row-group>
      </table:table>)");

  EXPECT_EQ(names_of(rows_of(table)), (std::vector<std::string>{"shown"}));
}

TEST(OdfTable, columns_inside_a_grouping_element) {
  pugi::xml_document document;
  const pugi::xml_node table = parse_table(document, R"(
      <table:table>
        <table:table-header-columns>
          <table:table-column id="a"/>
        </table:table-header-columns>
        <table:table-column-group>
          <table:table-column id="b"/>
        </table:table-column-group>
        <table:table-column id="c"/>
      </table:table>)");

  EXPECT_EQ(names_of(columns_of(table)),
            (std::vector<std::string>{"a", "b", "c"}));
}

TEST(OdfTable, a_row_in_a_group_is_not_dropped_from_a_text_document) {
  const std::string path = write_odf(
      "odf_table_header_rows.odt", "application/vnd.oasis.opendocument.text",
      std::string("<office:text>") + header_row_table + "</office:text>");

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<std::string> text = text_of(document);

  EXPECT_NE(std::ranges::find(text, "HEADER_A"), std::end(text));
  EXPECT_NE(std::ranges::find(text, "BODY_A"), std::end(text));
}

TEST(OdfTable, a_row_in_a_group_is_not_dropped_from_a_spreadsheet) {
  const std::string path =
      write_odf("odf_table_header_rows.ods",
                "application/vnd.oasis.opendocument.spreadsheet",
                std::string("<office:spreadsheet>") + header_row_table +
                    "</office:spreadsheet>");

  // a sheet's cells are off-tree, so they are read by position rather than
  // walked
  const Document document = odr::open(path).as_document_file().document();
  const Sheet sheet = document.root_element().first_child().as_sheet();
  ASSERT_TRUE(sheet);

  EXPECT_EQ(sheet.dimensions().rows, 2);

  std::vector<std::string> header;
  collect_text(sheet.cell(0, 0), header);
  EXPECT_EQ(header, (std::vector<std::string>{"HEADER_A"}));

  std::vector<std::string> body;
  collect_text(sheet.cell(0, 1), body);
  EXPECT_EQ(body, (std::vector<std::string>{"BODY_A"}));
}
