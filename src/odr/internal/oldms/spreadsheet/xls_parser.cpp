#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/spreadsheet/xls_element_registry.hpp>
#include <odr/internal/oldms/spreadsheet/xls_io.hpp>
#include <odr/internal/oldms/spreadsheet/xls_structs.hpp>
#include <odr/internal/oldms/spreadsheet/xls_style.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::oldms::spreadsheet {
namespace {

struct BoundSheet {
  std::uint32_t offset{0};
  std::string name;
};

/// The workbook-global style records collected from the globals substream.
struct GlobalStyles {
  std::vector<StyleRegistry::Font> fonts;
  std::vector<XfBody> xfs;
  /// The Palette record's colors; empty when the record is absent.
  std::vector<LongRgb> palette;
};

/// Creates a non-empty cell: sheet_cell → paragraph → text.
void add_cell(ElementRegistry &registry, const ElementIdentifier sheet_id,
              const std::uint32_t column, const std::uint32_t row,
              const std::uint16_t ixfe, std::string text) {
  auto [cell_id, cell_element, cell] =
      registry.create_sheet_cell_element(TablePosition(column, row));
  cell.ixfe = ixfe;
  registry.append_sheet_cell(sheet_id, cell_id);

  auto [paragraph_id, paragraph_element] =
      registry.create_element(ElementType::paragraph);
  registry.append_child(cell_id, paragraph_id);

  auto [text_id, text_element, text_entry] = registry.create_text_element();
  text_entry.text = std::move(text);
  registry.append_child(paragraph_id, text_id);
}

/// Globals substream: collects the worksheet BoundSheet8 entries, the shared
/// string table, and the style records (Font/XF/Palette).
void parse_globals(BiffReader &reader, std::vector<BoundSheet> &sheets,
                   std::vector<std::string> &shared_strings,
                   GlobalStyles &styles) {
  reader.expect_bof();

  while (reader.next_record() && reader.record_type() != biff_eof) {
    switch (reader.record_type()) {
    case biff_boundsheet: {
      const auto boundsheet = reader.read<BoundSheet8Fixed>();
      std::string name = reader.read_short_xl_unicode_string();
      if (boundsheet.dt == boundsheet_dt_worksheet) {
        sheets.push_back({boundsheet.lbPlyPos, std::move(name)});
      }
    } break;
    case biff_font: {
      const auto font = reader.read<FontFixed>();
      std::string name = reader.read_short_xl_unicode_string();
      styles.fonts.emplace_back(font, std::move(name));
    } break;
    case biff_xf: {
      styles.xfs.push_back(reader.read<XfBody>());
    } break;
    case biff_palette: {
      // ccv MUST be 56 ([MS-XLS] 2.4.188).
      const auto ccv = static_cast<std::int16_t>(reader.read_u16());
      if (ccv != palette_color_count) {
        throw std::runtime_error("xls: unexpected Palette color count");
      }
      styles.palette.resize(palette_color_count);
      for (LongRgb &color : styles.palette) {
        reader.read(color);
      }
    } break;
    case biff_sst: {
      const auto head = reader.read<SstHead>();
      if (head.cstUnique < 0) {
        throw std::runtime_error("xls: negative SST string count");
      }
      shared_strings.reserve(static_cast<std::size_t>(head.cstUnique));
      for (std::int32_t i = 0; i < head.cstUnique; ++i) {
        shared_strings.push_back(reader.read_xl_unicode_rich_extended_string());
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
  reader.expect_bof();

  ElementRegistry::Sheet &sheet = registry.sheet_element_at(sheet_id);
  sheet.name = info.name;

  // Set when a Formula record announces a string result; the value follows in
  // a String record ([MS-XLS] 2.5.133).
  struct PendingCell {
    TablePosition position;
    std::uint16_t ixfe;
  };
  std::optional<PendingCell> pending_string_cell;

  while (reader.next_record() && reader.record_type() != biff_eof) {
    switch (reader.record_type()) {
    case biff_dimensions: {
      const auto dimensions = reader.read<DimensionsBody>();
      sheet.dimensions = TableDimensions(dimensions.rwMac, dimensions.colMac);
    } break;
    case biff_labelsst: {
      const auto label = reader.read<LabelSstBody>();
      if (label.isst >= shared_strings.size()) {
        throw std::runtime_error("xls: SST index out of range");
      }
      add_cell(registry, sheet_id, label.cell.col, label.cell.rw,
               label.cell.ixfe, shared_strings[label.isst]);
    } break;
    case biff_rk: {
      const auto rk = reader.read<RkBody>();
      add_cell(registry, sheet_id, rk.col, rk.rw, rk.ixfe,
               format_number(rk.rk.decode()));
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
        const std::uint16_t ixfe = reader.read_u16();
        const auto rk = reader.read<RkNumber>();
        add_cell(registry, sheet_id, column_first + i, row, ixfe,
                 format_number(rk.decode()));
      }
    } break;
    case biff_number: {
      const auto number = reader.read<NumberBody>();
      add_cell(registry, sheet_id, number.cell.col, number.cell.rw,
               number.cell.ixfe, format_number(number.num));
    } break;
    case biff_label: {
      const auto cell = reader.read<CellRef>();
      add_cell(registry, sheet_id, cell.col, cell.rw, cell.ixfe,
               reader.read_xl_unicode_string());
    } break;
    case biff_boolerr: {
      const auto boolerr = reader.read<BoolErrBody>();
      add_cell(registry, sheet_id, boolerr.cell.col, boolerr.cell.rw,
               boolerr.cell.ixfe,
               boolerr.fError != 0
                   ? error_code_string(boolerr.bBoolErr)
                   : (boolerr.bBoolErr != 0 ? "TRUE" : "FALSE"));
    } break;
    case biff_formula: {
      const auto formula = reader.read<FormulaFixed>();
      const TablePosition position(formula.cell.col, formula.cell.rw);
      if (formula.val.is_xnum()) {
        const double value = formula.val.as_xnum();
        add_cell(registry, sheet_id, position.column, position.row,
                 formula.cell.ixfe, format_number(value));
      } else {
        switch (formula.val.type()) {
        case formula_value_string:
          pending_string_cell = {position, formula.cell.ixfe};
          break;
        case formula_value_boolean:
          add_cell(registry, sheet_id, position.column, position.row,
                   formula.cell.ixfe,
                   formula.val.bool_err_value() != 0 ? "TRUE" : "FALSE");
          break;
        case formula_value_error:
          add_cell(registry, sheet_id, position.column, position.row,
                   formula.cell.ixfe,
                   error_code_string(formula.val.bool_err_value()));
          break;
        case formula_value_blank:
          break;
        default:
          throw std::runtime_error("xls: unknown formula value type");
        }
      }
    } break;
    case biff_string: {
      if (pending_string_cell.has_value()) {
        add_cell(registry, sheet_id, pending_string_cell->position.column,
                 pending_string_cell->position.row, pending_string_cell->ixfe,
                 reader.read_xl_unicode_string());
        pending_string_cell.reset();
      }
    } break;
    default:
      break;
    }
  }
}

} // namespace
} // namespace odr::internal::oldms::spreadsheet

namespace odr::internal::oldms {

ElementIdentifier
spreadsheet::parse_tree(ElementRegistry &registry,
                        StyleRegistry &style_registry,
                        const abstract::ReadableFilesystem &files) {
  const auto workbook_stream = files.open(AbsPath("/Workbook"))->stream();
  BiffReader reader(*workbook_stream);

  std::vector<BoundSheet> bound_sheets;
  std::vector<std::string> shared_strings;
  GlobalStyles styles;
  parse_globals(reader, bound_sheets, shared_strings, styles);
  style_registry =
      StyleRegistry(std::move(styles.fonts), styles.xfs, styles.palette);

  auto [root_id, root] = registry.create_element(ElementType::root);

  for (const BoundSheet &bound_sheet : bound_sheets) {
    auto [sheet_id, sheet_element, sheet] = registry.create_sheet_element();
    registry.append_child(root_id, sheet_id);
    parse_sheet(reader, registry, sheet_id, bound_sheet, shared_strings);
  }

  return root_id;
}

} // namespace odr::internal::oldms
