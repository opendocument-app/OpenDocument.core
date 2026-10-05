#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <gtest/gtest.h>

#include <limits>

using namespace odr;

TEST(CellValue, a_default_value_states_nothing) {
  const CellValue value;

  EXPECT_EQ(value.type(), ValueType::unknown);
  EXPECT_FALSE(value.has_number());
  EXPECT_FALSE(value.has_text());
  EXPECT_FALSE(value.has_formula());
}

TEST(CellValue, a_text_value_types_itself_a_string) {
  const CellValue value("hello");

  EXPECT_EQ(value.type(), ValueType::string);
  EXPECT_EQ(value.text(), "hello");
  EXPECT_FALSE(value.has_number());
}

TEST(CellValue, a_number_value_shows_the_text_it_is_given) {
  const CellValue value(1234.5, "1 234,50");

  EXPECT_EQ(value.type(), ValueType::float_number);
  EXPECT_DOUBLE_EQ(value.number(), 1234.5);
  EXPECT_EQ(value.text(), "1 234,50");
}

/// The shortest spelling that reads back as the number, whatever the host's
/// locale.
TEST(CellValue, a_number_value_spells_itself_where_no_text_is_given) {
  EXPECT_EQ(CellValue(1234.5).text(), "1234.5");
  EXPECT_EQ(CellValue(2).text(), "2");
}

TEST(CellValue, asking_for_what_is_not_stated_throws) {
  const CellValue value("hello");

  EXPECT_THROW((void)value.number(), ValueNotStated);
  EXPECT_THROW((void)value.formula(), ValueNotStated);
}

/// A percentage states a number and is still typed a string, so the type is
/// not the number's to decide.
TEST(CellValue, a_type_outlives_what_is_put_beside_it) {
  const CellValue value =
      CellValue(ValueType::string).with_number(0.25).with_text("25%");

  EXPECT_EQ(value.type(), ValueType::string);
  EXPECT_DOUBLE_EQ(value.number(), 0.25);
}

TEST(CellValue, a_wither_leaves_the_value_it_was_asked_of_alone) {
  const CellValue value("hello");
  const CellValue with = value.with_formula("of:=A1");

  EXPECT_FALSE(value.has_formula());
  EXPECT_EQ(with.formula(), "of:=A1");
  EXPECT_EQ(with.text(), "hello");
}

TEST(SheetValue, nonfinite_numbers_leave_cells_and_dimensions_unchanged) {
  for (const FileType format :
       {FileType::opendocument_spreadsheet, FileType::office_open_xml_workbook,
        FileType::comma_separated_values}) {
    SCOPED_TRACE(file_type_to_string(format));
    const Document document = format == FileType::comma_separated_values
                                  ? open(File::from_memory("old,next\n1,2\n"),
                                         DecodeOptions::as(format))
                                        .as_csv_file()
                                        .document()
                                  : create_document(format);
    const Sheet sheet = document.root_element().first_child().as_sheet();
    sheet.set_cell(0, 0, CellValue("old"));
    const TableDimensions before = sheet.dimensions();
    for (const ValueType type : {ValueType::float_number, ValueType::boolean,
                                 ValueType::date, ValueType::time}) {
      for (const double number : {std::numeric_limits<double>::infinity(),
                                  -std::numeric_limits<double>::infinity(),
                                  std::numeric_limits<double>::quiet_NaN()}) {
        const CellValue value = CellValue(type).with_number(number);
        EXPECT_THROW(sheet.set_cell(0, 0, value), UnsupportedOperation);
        EXPECT_THROW(sheet.set_cell(9, 9, value), UnsupportedOperation);
        EXPECT_EQ(sheet.cell(0, 0).value().text(), "old");
        EXPECT_EQ(sheet.dimensions().rows, before.rows);
        EXPECT_EQ(sheet.dimensions().columns, before.columns);
      }
    }
  }
}
