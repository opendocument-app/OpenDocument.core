#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/spreadsheet/xls_element_registry.hpp>
#include <odr/internal/oldms/spreadsheet/xls_io.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>

#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace odr::internal::oldms::spreadsheet {

namespace {

struct BoundSheet {
  std::uint32_t offset{0};
  std::string name;
};

/// Creates a non-empty cell: sheet_cell → paragraph → text.
void add_cell(ElementRegistry &registry, const ElementIdentifier sheet_id,
              const std::uint32_t column, const std::uint32_t row,
              std::string text) {
  auto [cell_id, cell_element, cell] =
      registry.create_sheet_cell_element(TablePosition(column, row));
  registry.append_sheet_cell(sheet_id, cell_id);

  auto [paragraph_id, paragraph_element] =
      registry.create_element(ElementType::paragraph);
  registry.append_child(cell_id, paragraph_id);

  auto [text_id, text_element_, text_element] = registry.create_text_element();
  text_element.text = std::move(text);
  registry.append_child(paragraph_id, text_id);
}

void expect_bof(BiffReader &reader) {
  if (!reader.next_record() || reader.record_type() != biff_bof) {
    throw std::runtime_error("xls: expected BOF record");
  }
  BofFixed bof{};
  reader.read(bof);
  if (bof.vers != bof_vers_biff8) {
    throw std::runtime_error("xls: unsupported BIFF version " +
                             std::to_string(bof.vers));
  }
}

/// Globals substream: collects the worksheet BoundSheet8 entries and the
/// shared string table.
void parse_globals(BiffReader &reader, std::vector<BoundSheet> &sheets,
                   std::vector<std::string> &shared_strings) {
  expect_bof(reader);

  while (reader.next_record() && reader.record_type() != biff_eof) {
    switch (reader.record_type()) {
    case biff_boundsheet: {
      BoundSheet8Fixed boundsheet{};
      reader.read(boundsheet);
      std::string name = read_short_xl_unicode_string(reader);
      if (boundsheet.dt == boundsheet_dt_worksheet) {
        sheets.push_back({boundsheet.lbPlyPos, std::move(name)});
      }
    } break;
    case biff_sst: {
      SstHead head{};
      reader.read(head);
      if (head.cstUnique < 0) {
        throw std::runtime_error("xls: negative SST string count");
      }
      shared_strings.reserve(static_cast<std::size_t>(head.cstUnique));
      for (std::int32_t i = 0; i < head.cstUnique; ++i) {
        shared_strings.push_back(read_xl_unicode_rich_extended_string(reader));
      }
    } break;
    default:
      break;
    }
  }
}

/// Sheet substream: dimensions plus one element per non-empty cell.
void parse_sheet(BiffReader &reader, ElementRegistry &registry,
                 const ElementIdentifier sheet_id, const BoundSheet &info,
                 const std::vector<std::string> &shared_strings) {
  reader.seek(info.offset);
  expect_bof(reader);

  ElementRegistry::Sheet &sheet = registry.sheet_element_at(sheet_id);
  sheet.name = info.name;

  // Set when a Formula record announces a string result; the value follows in
  // a String record ([MS-XLS] 2.5.133).
  std::optional<TablePosition> pending_string_cell;

  while (reader.next_record() && reader.record_type() != biff_eof) {
    switch (reader.record_type()) {
    case biff_dimensions: {
      DimensionsBody dimensions{};
      reader.read(dimensions);
      sheet.dimensions = TableDimensions(dimensions.rwMac, dimensions.colMac);
    } break;
    case biff_labelsst: {
      LabelSstBody label{};
      reader.read(label);
      if (label.isst >= shared_strings.size()) {
        throw std::runtime_error("xls: SST index out of range");
      }
      add_cell(registry, sheet_id, label.cell.col, label.cell.rw,
               shared_strings[label.isst]);
    } break;
    case biff_rk: {
      RkBody rk{};
      reader.read(rk);
      add_cell(registry, sheet_id, rk.col, rk.rw,
               format_number(decode_rk(rk.rk)));
    } break;
    case biff_mulrk: {
      // rw, colFirst, (colLast - colFirst + 1) RkRecs, colLast
      // ([MS-XLS] 2.4.175); the cell count is derived from the body size.
      const std::uint16_t row = reader.read_u16();
      const std::uint16_t column_first = reader.read_u16();
      if (reader.remaining() < 2 || (reader.remaining() - 2) % 6 != 0) {
        throw std::runtime_error("xls: malformed MulRk record");
      }
      const std::size_t count = (reader.remaining() - 2) / 6;
      for (std::size_t i = 0; i < count; ++i) {
        reader.read_u16(); // ixfe
        const std::uint32_t rk = reader.read_u32();
        add_cell(registry, sheet_id, column_first + i, row,
                 format_number(decode_rk(rk)));
      }
    } break;
    case biff_number: {
      NumberBody number{};
      reader.read(number);
      add_cell(registry, sheet_id, number.cell.col, number.cell.rw,
               format_number(number.num));
    } break;
    case biff_label: {
      CellRef cell{};
      reader.read(cell);
      add_cell(registry, sheet_id, cell.col, cell.rw,
               read_xl_unicode_string(reader));
    } break;
    case biff_boolerr: {
      BoolErrBody boolerr{};
      reader.read(boolerr);
      add_cell(registry, sheet_id, boolerr.cell.col, boolerr.cell.rw,
               boolerr.fError != 0
                   ? error_code_string(boolerr.bBoolErr)
                   : (boolerr.bBoolErr != 0 ? "TRUE" : "FALSE"));
    } break;
    case biff_formula: {
      FormulaFixed formula{};
      reader.read(formula);
      const TablePosition position(formula.cell.col, formula.cell.rw);
      if (formula.val.fExprO != 0xFFFF) {
        double value;
        static_assert(sizeof(formula.val) == sizeof(value));
        std::memcpy(&value, &formula.val, sizeof(value));
        add_cell(registry, sheet_id, position.column, position.row,
                 format_number(value));
      } else {
        switch (formula.val.bytes[0]) {
        case 0x00: // string value in the following String record
          pending_string_cell = position;
          break;
        case 0x01: // boolean
          add_cell(registry, sheet_id, position.column, position.row,
                   formula.val.bytes[2] != 0 ? "TRUE" : "FALSE");
          break;
        case 0x02: // error
          add_cell(registry, sheet_id, position.column, position.row,
                   error_code_string(formula.val.bytes[2]));
          break;
        default: // 0x03 blank string
          break;
        }
      }
    } break;
    case biff_string: {
      if (pending_string_cell.has_value()) {
        add_cell(registry, sheet_id, pending_string_cell->column,
                 pending_string_cell->row, read_xl_unicode_string(reader));
        pending_string_cell.reset();
      }
    } break;
    default:
      break;
    }
  }
}

} // namespace

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files) {
  const auto workbook_stream = files.open(AbsPath("/Workbook"))->stream();
  BiffReader reader(*workbook_stream);

  std::vector<BoundSheet> bound_sheets;
  std::vector<std::string> shared_strings;
  parse_globals(reader, bound_sheets, shared_strings);

  auto [root_id, root] = registry.create_element(ElementType::root);

  for (const BoundSheet &bound_sheet : bound_sheets) {
    auto [sheet_id, sheet_element, sheet] = registry.create_sheet_element();
    registry.append_child(root_id, sheet_id);
    parse_sheet(reader, registry, sheet_id, bound_sheet, shared_strings);
  }

  return root_id;
}

} // namespace odr::internal::oldms::spreadsheet
