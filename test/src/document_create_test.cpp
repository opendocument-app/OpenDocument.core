#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;

namespace {

std::vector<Element> children(const Element &element) {
  std::vector<Element> result;
  for (const Element child : element.children()) {
    result.push_back(child);
  }
  return result;
}

/// The text of every run below @p element, at any depth.
std::string text_of(const Element &element) {
  if (element.type() == ElementType::text) {
    return element.as_text().content();
  }
  std::string result;
  for (const Element child : element.children()) {
    result += text_of(child);
  }
  return result;
}

/// The first run below @p element, at any depth.
Text first_text(const Element &element) {
  Element result = element;
  while (result && result.type() != ElementType::text) {
    result = result.first_child();
  }
  return result.as_text();
}

/// @p measure in centimetres, from the units the engines report.
double centimetres(const Measure &measure) {
  if (measure.unit().name() == "cm") {
    return measure.magnitude();
  }
  if (measure.unit().name() == "in") {
    return measure.magnitude() * 2.54;
  }
  throw std::invalid_argument("unexpected unit " + measure.unit().name());
}

Document reopen(const Document &document) {
  return open(document.save_to_memory(),
              DecodeOptions::as(document.file_type()))
      .as_document_file()
      .document();
}

} // namespace

TEST(DocumentCreate, refuses_a_type_without_the_capability) {
  for (const FileType type : all_file_types()) {
    if (capabilities_by_file_type(type).create) {
      continue;
    }
    EXPECT_THROW(std::ignore = create_document(type), UnsupportedFileType)
        << file_type_to_string(type);
  }
}

TEST(DocumentCreate, makes_every_type_with_the_capability) {
  for (const FileType type : all_file_types()) {
    if (!capabilities_by_file_type(type).create) {
      continue;
    }
    const Document document = create_document(type);
    EXPECT_EQ(document.file_type(), type) << file_type_to_string(type);
    EXPECT_EQ(document.document_type(), document_type_by_file_type(type))
        << file_type_to_string(type);
    EXPECT_TRUE(document.is_editable()) << file_type_to_string(type);
    EXPECT_TRUE(document.is_savable()) << file_type_to_string(type);
  }
}

TEST(DocumentCreate, renders_every_type_to_html) {
  for (const FileType type : all_file_types()) {
    if (!capabilities_by_file_type(type).create) {
      continue;
    }
    const Document document = create_document(type);
    HtmlConfig config;
    config.editable = true;
    const HtmlService service = html::translate(document, config);

    std::ostringstream out;
    service.list_views().at(0).write_html(out);
    EXPECT_NE(out.str().find("data-odr-id"), std::string::npos)
        << file_type_to_string(type);
  }
}

TEST(DocumentCreate, odt_holds_one_empty_paragraph) {
  const Document document = create_document(FileType::opendocument_text);

  const std::vector<Element> body = children(document.root_element());
  ASSERT_EQ(body.size(), 1);
  ASSERT_EQ(body[0].type(), ElementType::paragraph);
  EXPECT_TRUE(children(body[0]).empty());

  const TextStyle style = body[0].as_paragraph().text_style();
  ASSERT_TRUE(style.font_name.has_value());
  EXPECT_NE(style.font_name->find("Liberation Serif"), std::string::npos);
  EXPECT_EQ(style.font_size->to_string(), "12pt");
}

TEST(DocumentCreate, odt_page_is_a4) {
  const Document document = create_document(FileType::opendocument_text);

  const PageLayout layout =
      document.root_element().as_text_root().page_layout();
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
  EXPECT_EQ(layout.margin.left->to_string(), "2cm");
}

TEST(DocumentCreate, odt_takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(FileType::opendocument_text);
  const Paragraph first =
      (*document.root_element().children().begin()).as_paragraph();

  const Text hello = document.append_text(first, "hello");
  TextStyle bold;
  bold.font_weight = FontWeight::bold;
  hello.set_style(bold);
  const Paragraph second = document.insert_paragraph_after(first);
  std::ignore = document.append_text(second, "world");

  const Document saved = reopen(document);
  const std::vector<Element> body = children(saved.root_element());
  ASSERT_EQ(body.size(), 2);
  const Element span = children(body[0]).at(0);
  ASSERT_EQ(span.type(), ElementType::span);
  const Text saved_hello = children(span).at(0).as_text();
  EXPECT_EQ(saved_hello.content(), "hello");
  EXPECT_EQ(saved_hello.style().font_weight, FontWeight::bold);
  EXPECT_EQ(children(body[1]).at(0).as_text().content(), "world");
}

TEST(DocumentCreate, is_the_same_bytes_on_every_call) {
  for (const FileType type : all_file_types()) {
    if (!capabilities_by_file_type(type).create) {
      continue;
    }
    std::ostringstream first;
    std::ostringstream second;
    create_document(type).save(first);
    create_document(type).save(second);
    EXPECT_EQ(first.str(), second.str()) << file_type_to_string(type);
  }
}

TEST(DocumentCreate, ods_holds_one_empty_sheet) {
  const Document document = create_document(FileType::opendocument_spreadsheet);

  const std::vector<Element> sheets = children(document.root_element());
  ASSERT_EQ(sheets.size(), 1);
  ASSERT_EQ(sheets[0].type(), ElementType::sheet);
  const Sheet sheet = sheets[0].as_sheet();
  EXPECT_EQ(sheet.name(), "Sheet1");
  EXPECT_EQ(sheet.cell(0, 0).value_type(), ValueType::unknown);
}

TEST(DocumentCreate, ods_takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(FileType::opendocument_spreadsheet);
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  sheet.set_cell(0, 0, CellValue("hello"));
  sheet.set_cell(2, 4, CellValue(12.5));

  const Document saved = reopen(document);
  const Sheet saved_sheet =
      (*saved.root_element().children().begin()).as_sheet();
  EXPECT_EQ(saved_sheet.cell(0, 0).value().text(), "hello");
  EXPECT_EQ(saved_sheet.cell(2, 4).value().number(), 12.5);
  EXPECT_EQ(saved_sheet.dimensions().rows, 5);
  EXPECT_EQ(saved_sheet.dimensions().columns, 3);
}

TEST(DocumentCreate, docx_holds_one_empty_paragraph) {
  const Document document = create_document(FileType::office_open_xml_document);

  const std::vector<Element> body = children(document.root_element());
  ASSERT_EQ(body.size(), 1);
  ASSERT_EQ(body[0].type(), ElementType::paragraph);
  EXPECT_TRUE(children(body[0]).empty());

  const TextStyle style = body[0].as_paragraph().text_style();
  EXPECT_EQ(style.font_name, "Calibri");
  EXPECT_EQ(style.font_size->to_string(), "11pt");
}

TEST(DocumentCreate, docx_page_is_a4) {
  const Document document = create_document(FileType::office_open_xml_document);

  const PageLayout layout =
      document.root_element().as_text_root().page_layout();
  EXPECT_EQ(layout.width->to_string(), "8.268056in");
  EXPECT_EQ(layout.height->to_string(), "11.69306in");
  EXPECT_EQ(layout.margin.left->to_string(), "1in");
}

TEST(DocumentCreate, docx_takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(FileType::office_open_xml_document);
  const Paragraph first =
      (*document.root_element().children().begin()).as_paragraph();

  std::ignore = document.append_text(first, "hello");
  const Paragraph second = document.insert_paragraph_after(first);
  std::ignore = document.append_text(second, "world");

  const Document saved = reopen(document);
  const std::vector<Element> body = children(saved.root_element());
  ASSERT_EQ(body.size(), 2);
  EXPECT_EQ(text_of(body[0]), "hello");
  EXPECT_EQ(text_of(body[1]), "world");
}

TEST(DocumentCreate, xlsx_holds_one_empty_sheet) {
  const Document document = create_document(FileType::office_open_xml_workbook);

  const std::vector<Element> sheets = children(document.root_element());
  ASSERT_EQ(sheets.size(), 1);
  ASSERT_EQ(sheets[0].type(), ElementType::sheet);
  const Sheet sheet = sheets[0].as_sheet();
  EXPECT_EQ(sheet.name(), "Sheet1");
  EXPECT_EQ(sheet.cell(0, 0).value_type(), ValueType::unknown);
}

TEST(DocumentCreate, xlsx_takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(FileType::office_open_xml_workbook);
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  sheet.set_cell(0, 0, CellValue("hello"));
  sheet.set_cell(2, 4, CellValue(12.5));
  TextStyle bold;
  bold.font_weight = FontWeight::bold;
  sheet.set_cell_style(0, 0, {}, bold);

  const Document saved = reopen(document);
  const Sheet saved_sheet =
      (*saved.root_element().children().begin()).as_sheet();
  EXPECT_EQ(saved_sheet.cell(0, 0).value().text(), "hello");
  EXPECT_EQ(saved_sheet.cell(2, 4).value().number(), 12.5);
  EXPECT_EQ(saved_sheet.dimensions().rows, 5);
  EXPECT_EQ(saved_sheet.dimensions().columns, 3);
  EXPECT_EQ(first_text(saved_sheet.cell(0, 0)).style().font_weight,
            FontWeight::bold);
}

class DocumentCreatePresentation : public testing::TestWithParam<FileType> {};

INSTANTIATE_TEST_SUITE_P(
    DocumentCreate, DocumentCreatePresentation,
    testing::Values(FileType::opendocument_presentation,
                    FileType::office_open_xml_presentation),
    [](const testing::TestParamInfo<FileType> &info) {
      return file_type_to_string(info.param);
    });

TEST_P(DocumentCreatePresentation, holds_a_title_slide) {
  const Document document = create_document(GetParam());

  const std::vector<Element> slides = children(document.root_element());
  ASSERT_EQ(slides.size(), 1);
  ASSERT_EQ(slides[0].type(), ElementType::slide);

  const std::vector<Element> frames = children(slides[0]);
  ASSERT_EQ(frames.size(), 2);
  for (const Element &frame : frames) {
    ASSERT_EQ(frame.type(), ElementType::frame);
    EXPECT_EQ(text_of(frame), "");
  }
  EXPECT_NEAR(centimetres(*frames[0].as_frame().x()), 2.1, 1e-4);
  EXPECT_NEAR(centimetres(*frames[1].as_frame().width()), 21, 1e-4);
}

TEST_P(DocumentCreatePresentation, page_is_16_by_9) {
  const Document document = create_document(GetParam());

  const PageLayout layout =
      (*document.root_element().children().begin()).as_slide().page_layout();
  EXPECT_NEAR(centimetres(*layout.width), 28, 1e-4);
  EXPECT_NEAR(centimetres(*layout.height), 15.75, 1e-4);
}

TEST_P(DocumentCreatePresentation, takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(GetParam());
  const std::vector<Element> frames =
      children(*document.root_element().children().begin());
  ASSERT_EQ(frames.size(), 2);

  // the frame of a pptx holds a text body, the frame of an odp a text box
  Element paragraph = frames[0].first_child();
  while (paragraph && paragraph.type() != ElementType::paragraph) {
    paragraph = paragraph.first_child();
  }
  ASSERT_TRUE(paragraph);
  std::ignore = document.append_text(paragraph, "hello");

  const Document saved = reopen(document);
  const std::vector<Element> saved_frames =
      children(*saved.root_element().children().begin());
  ASSERT_EQ(saved_frames.size(), 2);
  EXPECT_EQ(text_of(saved_frames[0]), "hello");
  EXPECT_EQ(text_of(saved_frames[1]), "");
}

TEST(DocumentCreate, pptx_placeholders_take_the_style_of_layout_and_master) {
  const Document document =
      create_document(FileType::office_open_xml_presentation);
  const std::vector<Element> frames =
      children(*document.root_element().children().begin());
  ASSERT_EQ(frames.size(), 2);

  Element title = frames[0].first_child();
  while (title && title.type() != ElementType::paragraph) {
    title = title.first_child();
  }
  ASSERT_TRUE(title);
  const TextStyle text_style = title.as_paragraph().text_style();
  EXPECT_EQ(text_style.font_name, "Calibri Light");
  EXPECT_EQ(text_style.font_size->to_string(), "44pt");
  EXPECT_EQ(title.as_paragraph().style().text_align, TextAlign::center);
  EXPECT_EQ(frames[0].as_frame().style().vertical_align, VerticalAlign::bottom);
}
