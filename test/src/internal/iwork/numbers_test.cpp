#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_file.hpp>
#include <odr/internal/iwork/iwork_table.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <internal/iwork/iwork_element_util.hpp>
#include <internal/iwork/iwork_test_util.hpp>
#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;
using odr::test::TestData;
namespace builder = odr::test::iwork;
namespace iwork = odr::internal::iwork;

namespace {

/// A sheet's content extent, row by row.
std::vector<std::vector<std::string>> grid(const Sheet sheet) {
  const TableDimensions content = sheet.content({});

  std::vector<std::vector<std::string>> result;
  for (std::uint32_t row = 0; row < content.rows; ++row) {
    std::vector<std::string> cells;
    for (std::uint32_t column = 0; column < content.columns; ++column) {
      cells.push_back(builder::element_text(sheet.cell(column, row)));
    }
    result.push_back(std::move(cells));
  }
  return result;
}

std::vector<std::string> sheet_names(const Element root) {
  std::vector<std::string> result;
  for (const Element sheet : root.children()) {
    EXPECT_EQ(sheet.type(), ElementType::sheet);
    result.push_back(sheet.as_sheet().name());
  }
  return result;
}

/// The document a synthetic table decodes to.
Document numbers_document(const builder::TableSpec &spec,
                          const std::string &sheet_name = "Sheet",
                          const std::size_t table_repeats = 1) {
  return Document(std::make_shared<iwork::Document>(
      FileType::iwork_numbers,
      builder::numbers_package(spec, sheet_name, table_repeats)));
}

Sheet sheet_at(const Element root, const std::size_t index) {
  std::size_t i = 0;
  for (const Element sheet : root.children()) {
    if (i++ == index) {
      return sheet.as_sheet();
    }
  }
  return {};
}

} // namespace

TEST(IworkNumbers, is_detected_by_content) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);
  const std::string path =
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers");

  EXPECT_THAT(list_file_types(path, logger),
              testing::Contains(FileType::iwork_numbers));

  const DecodedFile file(path, logger);
  EXPECT_EQ(file.file_type(), FileType::iwork_numbers);
  EXPECT_EQ(file.file_category(), FileCategory::document);
  EXPECT_EQ(file.as_document_file().document_type(), DocumentType::spreadsheet);
}

// The blank template's one sheet holds one table with nothing in it, which
// must come back as a sheet of its declared extent and no content.
TEST(IworkNumbers, empty) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/empty.numbers"),
      Logger::null());
  EXPECT_EQ(document_file.file_type(), FileType::iwork_numbers);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::spreadsheet);
  EXPECT_FALSE(document.is_editable());

  const Element root = document.root_element();
  EXPECT_EQ(sheet_names(root), (std::vector<std::string>{"Sheet 1 – Table 1"}));

  const Sheet sheet = sheet_at(root, 0);
  EXPECT_EQ(sheet.dimensions().rows, 22);
  EXPECT_EQ(sheet.dimensions().columns, 7);
  EXPECT_EQ(sheet.content({}).rows, 0);
  EXPECT_EQ(sheet.content({}).columns, 0);
}

// A Numbers sheet holds many tables and our `Sheet` is one grid, so each
// table is a sheet of its own rather than only the first one surviving.
TEST(IworkNumbers, one_sheet_per_table) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(sheet_names(document.root_element()),
            (std::vector<std::string>{"Sales – Quarterly", "Sales – Wide",
                                      "Types – Values"}));
}

TEST(IworkNumbers, cell_values) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(grid(sheet_at(document.root_element(), 0)),
            (std::vector<std::vector<std::string>>{
                {"Quarter", "Revenue", "Growth"},
                {"Q1", "1000", "0.125"},
                {"Q2", "1250.5", "0.25"},
                // the cached value of `=SUM(B2:B3)` and `=AVERAGE(C2:C3)`;
                // `CalculationEngine.iwa` is not read
                {"Total", "2250.5", "0.1875"},
            }));
}

// Rows and columns cannot be confused: this table is three rows of six
// columns, and holds cells only at its corners.
TEST(IworkNumbers, a_table_wider_than_it_is_tall) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  const Sheet sheet = sheet_at(document.root_element(), 1);
  EXPECT_EQ(sheet.dimensions().rows, 3);
  EXPECT_EQ(sheet.dimensions().columns, 6);

  EXPECT_EQ(grid(sheet), (std::vector<std::vector<std::string>>{
                             {"a", "", "", "", "", "f"},
                             {"", "", "", "", "", ""},
                             {"bottom left", "", "", "", "", "bottom right"},
                         }));
}

TEST(IworkNumbers, every_cell_type_the_fixtures_hold) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(grid(sheet_at(document.root_element(), 2)),
            (std::vector<std::vector<std::string>>{
                {"Kind", "Value"},
                {"text", "A cell holding a rather longer piece of text"},
                {"number", "-42"},
                {"boolean", "TRUE"},
                {"date", "2024-01-01T10:30:00Z"},
                {"duration", "1h 30m"},
                // a row whose second cell the tile does not carry at all
                {"empty", ""},
                // Numbers shows 7.5%; the number format is not applied
                {"percent", "0.075"},
                {"two lines", "line one\nline two"},
            }));
}

// A number cell is right-aligned by the renderer; nothing else is.
TEST(IworkNumbers, only_a_number_reports_a_float_value_type) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();
  const Sheet sheet = sheet_at(document.root_element(), 2);

  EXPECT_EQ(sheet.cell(1, 2).value_type(), ValueType::float_number);
  EXPECT_EQ(sheet.cell(1, 1).value_type(), ValueType::string);
  EXPECT_EQ(sheet.cell(1, 4).value_type(), ValueType::string);
  // a position the tile carries no cell for
  EXPECT_EQ(sheet.cell(1, 6).value_type(), ValueType::unknown);
}

// A spreadsheet value is a decimal by construction, which is why Apple stores
// one; going through a `double` would put back the rounding it avoids.
TEST(IworkDecimal128, reads_the_values_the_fixtures_hold) {
  const auto decimal = [](const std::string &hex) {
    std::string bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
      bytes.push_back(
          static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return iwork::decimal128_to_string(bytes);
  };

  EXPECT_EQ(decimal("e8030000000000000000000000004030"), "1000");
  EXPECT_EQ(decimal("7d000000000000000000000000003a30"), "0.125");
  EXPECT_EQ(decimal("d9300000000000000000000000003e30"), "1250.5");
  EXPECT_EQ(decimal("2a000000000000000000000000004030"), "42");
  EXPECT_EQ(decimal("2a0000000000000000000000000040b0"), "-42");
  // Numbers stores 0.075 as 750000000000000e-16
  EXPECT_EQ(decimal("00e094fb1eaa02000000000000002030"), "0.075");
  EXPECT_EQ(decimal("00000000000000000000000000004030"), "0");
}

// IEEE 754 leaves a coefficient above 10^34 non-canonical and reads it as
// zero, and the same combination field is how an infinity or a NaN is written
// — none of which a cell should render as a number.
TEST(IworkDecimal128, a_non_canonical_combination_field_reads_as_zero) {
  const auto decimal = [](const std::string &hex) {
    std::string bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
      bytes.push_back(
          static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return iwork::decimal128_to_string(bytes);
  };

  // +infinity: the top bits of the last byte select the combination field
  EXPECT_EQ(decimal("00000000000000000000000000000078"), "0");
  // a coefficient too large to be canonical
  EXPECT_EQ(decimal("ffffffffffffffffffffffffffffff60"), "0");
}

TEST(IworkDecimal128, is_cut_off) {
  EXPECT_ANY_THROW(std::ignore = iwork::decimal128_to_string("short"));
}

// The instant stored is UTC, whatever the clock said when it was typed.
TEST(IworkDate, reads_seconds_since_2001) {
  EXPECT_EQ(iwork::date_to_string(0), "2001-01-01T00:00:00Z");
  EXPECT_EQ(iwork::date_to_string(725797800), "2024-01-01T10:30:00Z");
  EXPECT_EQ(iwork::date_to_string(-1), "2000-12-31T23:59:59Z");
}

// A cell carries its seconds as a raw double, so the value is the file's word:
// one no calendar can name is read as no date rather than cast into undefined
// behaviour.
TEST(IworkDate, a_value_no_calendar_can_name_is_no_date) {
  EXPECT_EQ(iwork::date_to_string(std::numeric_limits<double>::quiet_NaN()),
            "");
  EXPECT_EQ(iwork::date_to_string(std::numeric_limits<double>::infinity()), "");
  EXPECT_EQ(iwork::date_to_string(1e300), "");
  EXPECT_EQ(iwork::date_to_string(-1e300), "");
}

TEST(IworkDuration, reads_a_count_of_seconds) {
  EXPECT_EQ(iwork::duration_to_string(5400), "1h 30m");
  EXPECT_EQ(iwork::duration_to_string(0), "0s");
  EXPECT_EQ(iwork::duration_to_string(90061), "1d 1h 1m 1s");
  EXPECT_EQ(iwork::duration_to_string(-60), "-1m");
}

TEST(IworkDuration, a_value_no_count_can_hold_is_no_duration) {
  EXPECT_EQ(iwork::duration_to_string(std::numeric_limits<double>::quiet_NaN()),
            "");
  EXPECT_EQ(iwork::duration_to_string(1e300), "");
}

// The decision stage 6 rests on: a record declaring an encoding we have not
// mapped reads as an **empty** cell rather than a wrong one. Both bytes that
// can say so — the version and the type — take the same way out, and the
// control is the same record with the version this reader was written against.
TEST(IworkNumbers, a_record_we_have_not_mapped_is_an_empty_cell) {
  constexpr std::uint32_t decimal = iwork::cell::flag::decimal;
  // decimal128 `42`
  const std::string value("\x2a\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\x00\x00\x40\x30",
                          16);

  const auto document_of = [&](const std::string &record) {
    return numbers_document({.rows = 1,
                             .columns = 1,
                             .tile_rows = {builder::tile_row(0, {record})}});
  };
  // a cell points into the document it came from, so each one outlives its use
  const Document mapped = document_of(
      builder::cell_record(iwork::cell::type::number, decimal, value));
  const Document other_version = document_of(builder::cell_record(
      iwork::cell::type::number, decimal, value, iwork::cell::version + 1));
  const Document other_type =
      document_of(builder::cell_record(200, decimal, value));

  const auto cell = [](const Document &document) {
    return sheet_at(document.root_element(), 0).cell(0, 0);
  };

  EXPECT_EQ(builder::element_text(cell(mapped)), "42");

  // a version byte Apple has not shipped yet
  EXPECT_EQ(cell(other_version).as_sheet_cell().value_type(),
            ValueType::unknown);
  EXPECT_EQ(builder::element_text(cell(other_version)), "");

  // a type byte we have no reader for
  EXPECT_EQ(cell(other_type).as_sheet_cell().value_type(), ValueType::unknown);
  EXPECT_EQ(builder::element_text(cell(other_type)), "");
}

// The framing below a cell is the file's word about its own bytes, so a row
// that contradicts itself is a broken file rather than a shape we have not
// seen — the one place the tile reader throws instead of skipping.
TEST(IworkNumbers, a_row_that_contradicts_its_own_offsets_throws) {
  const std::string record =
      builder::cell_record(iwork::cell::type::number, 0, "");

  const auto read = [](const std::string &row) {
    std::ignore =
        numbers_document({.rows = 1, .columns = 2, .tile_rows = {row}});
  };

  EXPECT_THAT([&] { read(builder::tile_row_bytes(0, record, {0, 400})); },
              testing::ThrowsMessage<std::runtime_error>(
                  testing::HasSubstr("cell runs past its row storage")));

  EXPECT_THAT(
      [&] {
        read(builder::tile_row_bytes(
            0, record + record, {static_cast<std::int16_t>(record.size()), 0}));
      },
      testing::ThrowsMessage<std::runtime_error>(
          testing::HasSubstr("cell offsets are out of order")));
}

// A field the reader expects to be a message, written as a varint instead, is
// framing that contradicts itself rather than a shape we have not seen.
TEST(IworkNumbers, a_field_that_is_not_the_message_it_should_be_throws) {
  const auto throws = [](const std::string &message) {
    return testing::ThrowsMessage<std::runtime_error>(
        testing::HasSubstr(message));
  };

  builder::TableSpec spec{.rows = 1, .columns = 1};
  spec.tile_rows = {};

  // a tile whose row list holds a varint
  EXPECT_THAT(
      [&] {
        builder::TableSpec malformed = spec;
        malformed.raw_tile = builder::number_field(iwork::tile::rows, 1);
        std::ignore = numbers_document(malformed);
      },
      throws("malformed tile row"));

  // a tile storage whose tile list holds a varint
  EXPECT_THAT(
      [&] {
        builder::TableSpec malformed = spec;
        malformed.raw_tile_list =
            builder::number_field(iwork::tile_storage::tiles, 1);
        std::ignore = numbers_document(malformed);
      },
      throws("malformed tile list"));

  // a data list whose entry list holds a varint
  EXPECT_THAT(
      [&] {
        builder::TableSpec malformed = spec;
        malformed.strings = {
            builder::number_field(iwork::data_list::entries, 1)};
        std::ignore = numbers_document(malformed);
      },
      throws("malformed data list"));
}

// A tile list may name one tile any number of times, and `Package::object`
// hands every repeat back from its cache — so the cells a repeat appends are
// spent as they are produced rather than once the model is complete.
TEST(IworkNumbers, a_repeated_tile_is_capped_by_the_cells_it_carries) {
  const std::string record = builder::cell_record(
      iwork::cell::type::string, iwork::cell::flag::string_key,
      std::string("\x01\x00\x00\x00", 4));

  EXPECT_THAT(
      [&] {
        std::ignore = numbers_document(
            {.rows = 1,
             .columns = 1,
             .tile_rows = {builder::tile_row(0, {record})},
             .strings = {builder::string_entry(1, std::string(1024, 'a'))},
             .tile_repeats = 100'000});
      },
      testing::ThrowsMessage<std::runtime_error>(
          testing::HasSubstr("too much text")));
}
