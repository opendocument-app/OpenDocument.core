#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/quantity.hpp>
#include <odr/slide_layout.hpp>
#include <odr/style.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;

namespace {

std::vector<std::string> sheet_names(const Document &document) {
  std::vector<std::string> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_sheet().name());
  }
  return result;
}

std::vector<std::string> layout_names(const Document &document) {
  std::vector<std::string> result;
  for (const SlideLayout &layout : document.slide_layouts()) {
    result.push_back(layout.name());
  }
  return result;
}

Document reopen(const Document &document) {
  return open(document.save_to_memory(),
              DecodeOptions::as(document.file_type()))
      .as_document_file()
      .document();
}

class DocumentInsertSheet : public testing::TestWithParam<FileType> {};

} // namespace

INSTANTIATE_TEST_SUITE_P(DocumentStructure, DocumentInsertSheet,
                         testing::Values(FileType::opendocument_spreadsheet,
                                         FileType::office_open_xml_workbook),
                         [](const testing::TestParamInfo<FileType> &info) {
                           return file_type_to_string(info.param);
                         });

TEST_P(DocumentInsertSheet, appends_and_prepends_in_order) {
  const Document document = create_document(GetParam());

  const Sheet last = document.insert_sheet(1, "Last");
  const Sheet first = document.insert_sheet(0, "First");

  EXPECT_EQ(last.name(), "Last");
  EXPECT_EQ(first.name(), "First");
  EXPECT_EQ(sheet_names(document),
            (std::vector<std::string>{"First", "Sheet1", "Last"}));
}

TEST_P(DocumentInsertSheet, an_empty_name_takes_the_first_free_one) {
  const Document document = create_document(GetParam());

  EXPECT_EQ(document.insert_sheet(1, "").name(), "Sheet2");
  std::ignore = document.insert_sheet(2, "Sheet4");
  EXPECT_EQ(document.insert_sheet(3, "").name(), "Sheet5");
}

TEST_P(DocumentInsertSheet, refuses_a_taken_or_illegal_name) {
  const Document document = create_document(GetParam());

  EXPECT_THROW(std::ignore = document.insert_sheet(1, "sheet1"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, "a/b"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, "'quoted'"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, std::string(32, 'x')),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(2, "Far"),
               std::out_of_range);
  EXPECT_EQ(sheet_names(document), std::vector<std::string>{"Sheet1"});
}

TEST_P(DocumentInsertSheet, a_new_sheet_takes_cells_and_survives_a_save) {
  const Document document = create_document(GetParam());

  const Sheet added = document.insert_sheet(0, "Data");
  added.set_cell(1, 2, CellValue("hello"));
  added.set_cell(0, 0, CellValue(42.0));

  const Document saved = reopen(document);
  EXPECT_EQ(sheet_names(saved), (std::vector<std::string>{"Data", "Sheet1"}));
  const Sheet data = (*saved.root_element().children().begin()).as_sheet();
  EXPECT_EQ(data.cell(1, 2).value().text(), "hello");
  EXPECT_EQ(data.cell(0, 0).value().number(), 42.0);
}

TEST_P(DocumentInsertSheet, a_formula_reads_a_sheet_inserted_after_it) {
  const Document document = create_document(GetParam());
  const Sheet first = (*document.root_element().children().begin()).as_sheet();

  const Sheet data = document.insert_sheet(1, "Data");
  data.set_cell(0, 0, CellValue(7.0));
  first.set_cell(
      0, 0,
      CellValue().with_formula(GetParam() == FileType::opendocument_spreadsheet
                                   ? "of:=[$Data.A1]*2"
                                   : "Data!A1*2"));
  std::ignore = document.recalculate();

  EXPECT_EQ(first.cell(0, 0).value().number(), 14.0);
}

TEST(DocumentStructure, a_text_document_refuses_a_sheet) {
  const Document document = create_document(FileType::opendocument_text);
  EXPECT_THROW(std::ignore = document.insert_sheet(0, "Sheet1"),
               UnsupportedOperation);
}

namespace {

class DocumentInsertSlide : public testing::TestWithParam<FileType> {};

std::vector<Element> children_of(const Element &element) {
  std::vector<Element> result;
  for (const Element child : element.children()) {
    result.push_back(child);
  }
  return result;
}

Element first_paragraph(const Element &element) {
  Element result = element.first_child();
  while (result && result.type() != ElementType::paragraph) {
    result = result.first_child();
  }
  return result;
}

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

double centimetres(const Measure &measure) {
  return measure.unit().name() == "in" ? measure.magnitude() * 2.54
                                       : measure.magnitude();
}

} // namespace

INSTANTIATE_TEST_SUITE_P(
    DocumentStructure, DocumentInsertSlide,
    testing::Values(FileType::opendocument_presentation,
                    FileType::office_open_xml_presentation),
    [](const testing::TestParamInfo<FileType> &info) {
      return file_type_to_string(info.param);
    });

TEST_P(DocumentInsertSlide, a_new_presentation_offers_the_six_layouts) {
  const Document document = create_document(GetParam());
  EXPECT_EQ(layout_names(document),
            (std::vector<std::string>{"Title Slide", "Title and Content",
                                      "Section Header", "Two Content",
                                      "Title Only", "Blank"}));
}

TEST_P(DocumentInsertSlide, a_slide_holds_its_layouts_placeholders) {
  const Document document = create_document(GetParam());

  const Slide added = document.insert_slide(1, 3);

  const std::vector<Element> slides = children_of(document.root_element());
  ASSERT_EQ(slides.size(), 2);
  EXPECT_EQ(slides[1], Element(added));
  const std::vector<Element> frames = children_of(added);
  ASSERT_EQ(frames.size(), 3);
  for (const Element &frame : frames) {
    EXPECT_TRUE(first_paragraph(frame));
  }
  EXPECT_NEAR(centimetres(*frames[2].as_frame().x()), 14.3, 1e-3);
  EXPECT_NEAR(centimetres(*frames[2].as_frame().width()), 12.3, 1e-3);
}

TEST_P(DocumentInsertSlide, a_layout_lists_the_placeholders_a_slide_gets) {
  const Document document = create_document(GetParam());
  const std::vector<SlideLayout> layouts = document.slide_layouts();

  const std::vector<SlidePlaceholder> &title = layouts[0].placeholders();
  ASSERT_EQ(title.size(), 2);
  EXPECT_EQ(title[0].kind(), SlidePlaceholderKind::title);
  EXPECT_EQ(title[1].kind(), SlidePlaceholderKind::subtitle);
  EXPECT_TRUE(layouts[5].placeholders().empty());

  for (std::uint32_t i = 0; i < layouts.size(); ++i) {
    const std::vector<Element> frames =
        children_of(document.insert_slide(1, i));
    const std::vector<SlidePlaceholder> &placeholders =
        layouts[i].placeholders();
    ASSERT_EQ(frames.size(), placeholders.size()) << layouts[i].name();
    for (std::size_t j = 0; j < frames.size(); ++j) {
      EXPECT_NEAR(centimetres(*frames[j].as_frame().x()),
                  centimetres(placeholders[j].x()), 1e-3);
      EXPECT_NEAR(centimetres(*frames[j].as_frame().height()),
                  centimetres(placeholders[j].height()), 1e-3);
    }
  }
}

TEST_P(DocumentInsertSlide, a_blank_slide_goes_first) {
  const Document document = create_document(GetParam());

  std::ignore = document.insert_slide(0, 5);

  const std::vector<Element> slides = children_of(document.root_element());
  ASSERT_EQ(slides.size(), 2);
  EXPECT_TRUE(children_of(slides[0]).empty());
  EXPECT_EQ(children_of(slides[1]).size(), 2);
}

TEST_P(DocumentInsertSlide, a_new_slide_takes_text_and_survives_a_save) {
  const Document document = create_document(GetParam());

  const Slide added = document.insert_slide(1, 1);
  std::ignore = document.append_text(first_paragraph(*added.children().begin()),
                                     "agenda");
  std::ignore = document.insert_slide(2, 4);

  const Document saved = reopen(document);
  const std::vector<Element> slides = children_of(saved.root_element());
  ASSERT_EQ(slides.size(), 3);
  EXPECT_EQ(text_of(slides[1]), "agenda");
  EXPECT_EQ(children_of(slides[2]).size(), 1);
  EXPECT_EQ(saved.slide_layouts().size(), 6);
}

TEST_P(DocumentInsertSlide, refuses_an_index_or_layout_past_the_end) {
  const Document document = create_document(GetParam());

  EXPECT_THROW(std::ignore = document.insert_slide(2, 0), std::out_of_range);
  EXPECT_THROW(std::ignore = document.insert_slide(1, 6), std::out_of_range);
  EXPECT_EQ(children_of(document.root_element()).size(), 1);
}

TEST(DocumentStructure, a_text_document_offers_no_layout) {
  const Document document = create_document(FileType::opendocument_text);
  EXPECT_TRUE(document.slide_layouts().empty());
  EXPECT_THROW(std::ignore = document.insert_slide(0, 0), UnsupportedOperation);
}

TEST(DocumentStructure, an_odp_offers_its_own_layouts_then_the_built_ins) {
  const Document document =
      open(test::TestData::test_file_path("odr-public/odp/empty.odp"))
          .as_document_file()
          .document();
  // `AL1T0` holds a title and a subtitle; the handout layout is left out
  EXPECT_EQ(layout_names(document),
            (std::vector<std::string>{"Title Slide", "Title and Content",
                                      "Section Header", "Two Content",
                                      "Title Only", "Blank"}));

  EXPECT_EQ(document.slide_layouts()[0].placeholders().size(), 2);

  const Slide added = document.insert_slide(1, 3);
  EXPECT_EQ(children_of(added).size(), 3);

  const Document saved = reopen(document);
  EXPECT_EQ(saved.slide_layouts().size(), 6);
  EXPECT_EQ(children_of(saved.root_element()).size(), 2);
}

TEST(DocumentStructure, a_pptx_with_two_masters_numbers_a_repeated_layout) {
  const Document document = open(test::TestData::test_file_path(
                                     "odr-public/pptx/style-various-1.pptx"))
                                .as_document_file()
                                .document();
  const std::vector<std::string> layouts = layout_names(document);
  ASSERT_EQ(layouts.size(), 24);
  EXPECT_EQ(layouts[1], "Title Slide");
  EXPECT_EQ(layouts[13], "Title Slide (2)");

  const Slide added = document.insert_slide(1, 2);
  const Document saved = reopen(document);
  EXPECT_EQ(children_of(saved.root_element()).size(),
            children_of(document.root_element()).size());
  EXPECT_EQ(children_of(added).size(), 2);
}

namespace {

class DocumentInsertTextBox : public testing::TestWithParam<FileType> {};

} // namespace

INSTANTIATE_TEST_SUITE_P(
    DocumentStructure, DocumentInsertTextBox,
    testing::Values(FileType::opendocument_presentation,
                    FileType::office_open_xml_presentation),
    [](const testing::TestParamInfo<FileType> &info) {
      return file_type_to_string(info.param);
    });

TEST_P(DocumentInsertTextBox, sits_where_it_was_put_and_draws_nothing) {
  const Document document = create_document(GetParam());
  const Element slide = *document.root_element().children().begin();

  const Frame box = document.insert_text_box(
      slide, Measure("2cm"), Measure("3cm"), Measure("10cm"), Measure("1.5cm"));

  const std::vector<Element> frames = children_of(slide);
  ASSERT_EQ(frames.size(), 3);
  EXPECT_EQ(frames[2], Element(box));
  EXPECT_NEAR(centimetres(*box.x()), 2, 1e-3);
  EXPECT_NEAR(centimetres(*box.y()), 3, 1e-3);
  EXPECT_NEAR(centimetres(*box.width()), 10, 1e-3);
  EXPECT_NEAR(centimetres(*box.height()), 1.5, 1e-3);
  EXPECT_TRUE(first_paragraph(box));
  const GraphicStyle style = box.style();
  EXPECT_TRUE(!style.fill_color.has_value() || style.fill_color->alpha == 0);
}

TEST_P(DocumentInsertTextBox, takes_text_and_survives_a_save) {
  const Document document = create_document(GetParam());
  const Slide added = document.insert_slide(1, 5);

  const Frame box = document.insert_text_box(
      added, Measure("1in"), Measure("1in"), Measure("4in"), Measure("1in"));
  std::ignore = document.append_text(first_paragraph(box), "note");
  std::ignore = document.insert_text_box(added, Measure("1in"), Measure("3in"),
                                         Measure("4in"), Measure("1in"));

  const Document saved = reopen(document);
  const std::vector<Element> slides = children_of(saved.root_element());
  ASSERT_EQ(slides.size(), 2);
  const std::vector<Element> frames = children_of(slides[1]);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(text_of(frames[0]), "note");
  EXPECT_NEAR(centimetres(*frames[1].as_frame().y()), 3 * 2.54, 1e-3);
}

TEST_P(DocumentInsertTextBox, refuses_a_relative_length_or_an_empty_size) {
  const Document document = create_document(GetParam());
  const Element slide = *document.root_element().children().begin();

  EXPECT_THROW(std::ignore = document.insert_text_box(
                   slide, Measure("1em"), Measure("0cm"), Measure("1cm"),
                   Measure("1cm")),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_text_box(
                   slide, Measure("0cm"), Measure("0cm"), Measure("0cm"),
                   Measure("1cm")),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_text_box(
                   first_paragraph(*slide.children().begin()), Measure("0cm"),
                   Measure("0cm"), Measure("1cm"), Measure("1cm")),
               UnsupportedOperation);
  EXPECT_EQ(children_of(slide).size(), 2);
}

TEST(DocumentStructure, a_text_document_refuses_a_text_box) {
  const Document document = create_document(FileType::opendocument_text);
  EXPECT_THROW(std::ignore = document.insert_text_box(
                   *document.root_element().children().begin(), Measure("0cm"),
                   Measure("0cm"), Measure("1cm"), Measure("1cm")),
               UnsupportedOperation);
}

TEST(DocumentStructure, an_odg_page_takes_a_text_box) {
  const Document document =
      open(test::TestData::test_file_path("odr-public/odg/empty.odg"))
          .as_document_file()
          .document();
  const Element page = *document.root_element().children().begin();
  ASSERT_EQ(page.type(), ElementType::page);

  const Frame box = document.insert_text_box(
      page, Measure("2cm"), Measure("2cm"), Measure("8cm"), Measure("2cm"));
  std::ignore = document.append_text(first_paragraph(box), "drawn");

  const Document saved = reopen(document);
  EXPECT_EQ(text_of(*saved.root_element().children().begin()), "drawn");
}

TEST(DocumentStructure, a_pptx_text_box_takes_the_theme_body_font) {
  const Document document =
      create_document(FileType::office_open_xml_presentation);
  const Frame box = document.insert_text_box(
      *document.root_element().children().begin(), Measure("1cm"),
      Measure("1cm"), Measure("5cm"), Measure("1cm"));

  const TextStyle style = first_paragraph(box).as_paragraph().text_style();
  EXPECT_EQ(style.font_name, "Calibri");
  EXPECT_EQ(style.font_size->to_string(), "18pt");
}
