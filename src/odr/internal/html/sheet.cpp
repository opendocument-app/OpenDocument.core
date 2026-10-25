#include <odr/internal/html/sheet.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/formula/formula_dependencies.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/style_registry.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace odr::internal {

TableDimensions html::sheet_rendered_extent(const Sheet &sheet,
                                            const HtmlConfig &config) {
  const TableDimensions dimensions = sheet.dimensions();

  std::uint32_t end_column = dimensions.columns;
  std::uint32_t end_row = dimensions.rows;
  if (config.spreadsheet_limit_by_content) {
    // Not the sheet's own extent clamped: a cell past the window does not
    // stretch what precedes it.
    const TableDimensions content = sheet.content(config.spreadsheet_limit);
    end_column = content.columns;
    end_row = content.rows;
  }
  if (config.spreadsheet_limit.has_value()) {
    end_column = std::min(end_column, config.spreadsheet_limit->columns);
    end_row = std::min(end_row, config.spreadsheet_limit->rows);
  }
  end_column = std::max(1u, end_column);
  if (config.spreadsheet_cell_limit.has_value()) {
    const std::uint64_t limit =
        std::max<std::uint64_t>(1, *config.spreadsheet_cell_limit);
    end_column =
        static_cast<std::uint32_t>(std::min<std::uint64_t>(end_column, limit));
    const std::uint64_t rows = limit / end_column;
    end_row = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(end_row, std::max<std::uint64_t>(1, rows)));
  }
  end_row = std::max(1u, end_row);

  return {end_row, end_column};
}

namespace {

/// What a sheet reads to give a column that states no width one.
constexpr std::uint32_t estimated_rows = 1000;
/// About the width of a character of the sheet's font, and the padding of a
/// cell, which a column stating no width is estimated with.
constexpr double character_pixels = 7.5;
constexpr double cell_padding_pixels = 14;
/// About the width of a digit of a row label, and the padding around one.
constexpr double digit_pixels = 7.5;
constexpr double gutter_padding_pixels = 14;
/// What a row that states no height is estimated at, before it is laid out.
constexpr double estimated_row_pixels = 20;
/// A chunk of a sheet holds about as many cells (`lazy-loading.md` decision 3).
constexpr std::uint32_t chunk_cells = 4096;

/// How far a sheet has to shrink to fit the paper the file states; nothing
/// where it fits already, or where no width is stated.
std::optional<double> sheet_print_fit(const Sheet &sheet,
                                      const std::uint32_t end_column) {
  const PageLayout page_layout = sheet.page_layout();
  const std::optional<double> page = html::css_pixels(page_layout.width);
  if (!page.has_value()) {
    return {};
  }
  const double printable =
      *page - html::css_pixels(page_layout.margin.left).value_or(0) -
      html::css_pixels(page_layout.margin.right).value_or(0);

  // the ruler does not print
  double content = 0;
  for (std::uint32_t column = 0; column < end_column; ++column) {
    const std::optional<double> width =
        html::css_pixels(sheet.column_style(column).width);
    if (!width.has_value()) {
      return {};
    }
    content += *width;
  }

  if (printable <= 0 || content <= printable) {
    return {};
  }
  return printable / content;
}

/// A run whose style the box around it can carry instead. Not a background, a
/// raised run or an addressed one: each means something else on the box.
std::optional<Text> plain_text(const Element &element,
                               const html::WritingState &state) {
  if (element.type() != ElementType::text) {
    return {};
  }
  if (html::writes_edit_markup(element, state)) {
    return {};
  }

  const Text text = element.as_text();
  if (text.content().empty()) {
    return {};
  }
  const TextStyle style = text.style();
  if (style.background_color.has_value() || style.font_position.has_value()) {
    return {};
  }
  return text;
}

/// @ref plain_text where @p paragraph holds one and nothing else.
std::optional<Text> plain_run(const Paragraph &paragraph,
                              const html::WritingState &state) {
  const ElementRange children = paragraph.children();
  ElementIterator child = children.begin();
  if (child == children.end()) {
    return {};
  }
  const Element element = *child;
  if (++child != children.end()) {
    return {};
  }
  return plain_text(element, state);
}

/// Nothing a reader would see, so the cell beside it may spill over it.
bool is_blank(const SheetCell &cell) {
  for (const Element child : cell.children()) {
    if (child.type() != ElementType::paragraph ||
        html::has_content(child.children())) {
      return false;
    }
  }
  return true;
}

/// Text, and spans of text, and nothing else, all the way down.
bool holds_plain_runs(const ElementRange &children) {
  for (const Element child : children) {
    const ElementType type = child.type();
    if (type == ElementType::text || type == ElementType::line_break) {
      continue;
    }
    if (type != ElementType::span || !holds_plain_runs(child.children())) {
      return false;
    }
  }
  return true;
}

/// What a write can replace: text, line breaks and spans of them, under
/// paragraphs or not. odf wraps each line of a cell in a `text:p`, and ooxml
/// hangs the text under the `c` directly.
bool holds_plain_cell(const ElementRange &children) {
  ElementIterator child = children.begin();
  if (child == children.end() || (*child).type() != ElementType::paragraph) {
    return holds_plain_runs(children);
  }
  return std::ranges::all_of(children, [](const Element paragraph) {
    return paragraph.type() == ElementType::paragraph &&
           holds_plain_runs(paragraph.children());
  });
}

/// The text of the runs under @p element.
std::string shown_text(const Element &element) {
  std::string result;
  for (const Element child : element.children()) {
    result += child.type() == ElementType::text ? child.as_text().content()
                                                : shown_text(child);
  }
  return result;
}

/// The number the editor opens a cell on and a sort orders it by, where the
/// cell shows it formatted: a date and a time always.
std::optional<std::string> formatted_value(const CellValue &value,
                                           const SheetCell &cell) {
  if (!value.has_number()) {
    return std::nullopt;
  }
  if (value.type() == ValueType::date || value.type() == ValueType::time) {
    return fmt::format("{}", value.number());
  }
  if (value.type() != ValueType::float_number) {
    return std::nullopt;
  }
  std::string plain = fmt::format("{}", value.number());
  if (shown_text(cell) == plain) {
    return std::nullopt;
  }
  return plain;
}

/// Its place among the document's sheets, which is how an op names one.
std::uint32_t sheet_ordinal(const Sheet &sheet) {
  std::uint32_t ordinal = 0;
  for (Element previous = sheet.previous_sibling(); previous;
       previous = previous.previous_sibling()) {
    ++ordinal;
  }
  return ordinal;
}

/// Every sheet by its name without case, as a formula names one, so a
/// reference into another sheet resolves to the ordinal an op uses.
std::unordered_map<std::string, std::uint32_t>
sheet_ordinals_by_name(const Sheet &sheet) {
  Element first = sheet;
  for (Element previous = sheet.previous_sibling(); previous;
       previous = previous.previous_sibling()) {
    first = previous;
  }

  std::unordered_map<std::string, std::uint32_t> result;
  std::uint32_t ordinal = 0;
  for (Element current = first; current; current = current.next_sibling()) {
    if (current.type() == ElementType::sheet) {
      result.emplace(util::string::to_lower(current.as_sheet().name()),
                     ordinal);
    }
    ++ordinal;
  }
  return result;
}

/// Space-separated `sheet,first column,last column,first row,last row` boxes.
/// `*` is unbounded; unresolved reads cover every edit on this sheet.
std::string
cell_reads(const std::string &formula, const formula::Syntax syntax,
           const std::uint32_t own_sheet,
           const std::unordered_map<std::string, std::uint32_t> &by_name,
           const std::vector<formula::Name> *names) {
  const std::optional<formula::Node> node = formula::parse(formula, syntax);
  if (!node.has_value()) {
    return std::to_string(own_sheet) + ",0,*,0,*";
  }
  return formula::spell_reads(
      *node, own_sheet,
      [&by_name](const std::string_view name) -> std::optional<std::uint32_t> {
        const auto named = by_name.find(util::string::to_lower(name));
        return named == by_name.end() ? std::nullopt
                                      : std::optional(named->second);
      },
      [&](const std::string_view name) -> std::optional<formula::Node> {
        const formula::Name *named =
            names != nullptr ? formula::find_name(*names, name, own_sheet)
                             : nullptr;
        return named != nullptr ? formula::parse(named->expression, syntax)
                                : std::nullopt;
      });
}

/// Why a cell cannot be edited, or null where it can be. The names the page
/// reports to its host; `spreadsheet-editing.md` decision 3 lists them. A
/// formula is locked where the page has no evaluator to open it with.
const char *cell_lock(const CellValue &value, const SheetCell &cell,
                      const bool anchors_shapes) {
  if (value.has_formula() && !html::carries_formula_module()) {
    return "formula";
  }
  // its drawings are what the cell is, and an overlay would cover them
  if (anchors_shapes) {
    return "shapes";
  }
  // a write replaces the cell's runs, and a link would go unseen with them
  return holds_plain_cell(cell.children()) ? nullptr : "rich";
}

/// A shape or picture anchored in a cell reaches past it by design.
bool holds_only_text(const SheetCell &cell) {
  for (const Element child : cell.children()) {
    switch (child.type()) {
    case ElementType::paragraph:
    case ElementType::text:
    case ElementType::span:
    case ElementType::link:
    case ElementType::bookmark:
    case ElementType::line_break:
      break;
    default:
      return false;
    }
  }
  return true;
}

bool is_zero(const std::optional<Quantity<double>> &margin) {
  return !margin.has_value() || margin->magnitude() == 0;
}

/// What the run computed to, under the two properties the paragraph's block
/// carries.
TextStyle run_style(const Paragraph &paragraph, const Text &run) {
  TextStyle result;
  result.font_name = paragraph.text_style().font_name;
  result.font_size = paragraph.text_style().font_size;
  result.override(run.style());
  return result;
}

struct FoldedCell {
  std::string style;
  std::string text;
};

/// The `td` carries the styles of the boxes it stands in for. A stated row
/// height keeps the paragraph: `contain:size`, `max-height`, `overflow` and
/// `content-visibility` are all ignored on a table cell.
std::optional<FoldedCell> fold_cell(const SheetCell &cell,
                                    const html::WritingState &state,
                                    const bool wraps, const bool anchors_shapes,
                                    const std::optional<Measure> &row_height) {
  if (wraps || anchors_shapes) {
    return {};
  }

  const ElementRange children = cell.children();
  ElementIterator child = children.begin();
  if (child == children.end()) {
    return {};
  }
  const Element only = *child;
  if (++child != children.end()) {
    return {};
  }

  if (only.type() == ElementType::text) {
    const std::optional<Text> run = plain_text(only, state);
    if (!run.has_value()) {
      return {};
    }
    return FoldedCell{html::translate_text_style(run->style()),
                      xml::escape_text(run->content())};
  }

  if (only.type() != ElementType::paragraph || row_height.has_value()) {
    return {};
  }
  const Paragraph paragraph = only.as_paragraph();
  const std::optional<Text> run = plain_run(paragraph, state);
  if (!run.has_value()) {
    return {};
  }
  const ParagraphStyle style = paragraph.style();
  if (!is_zero(style.margin.left) || !is_zero(style.margin.right) ||
      !is_zero(style.margin.top) || !is_zero(style.margin.bottom)) {
    return {};
  }

  return FoldedCell{html::translate_paragraph_style(style, state.direction()) +
                        html::translate_text_style(run_style(paragraph, *run)),
                    xml::escape_text(run->content())};
}

/// A paragraph holding one plain string carries what the run's `x-s` carried.
void translate_cell_children(const SheetCell &cell,
                             const html::WritingState &state) {
  for (const Element child : cell.children()) {
    const std::optional<Text> run = child.type() == ElementType::paragraph
                                        ? plain_run(child.as_paragraph(), state)
                                        : std::nullopt;
    if (!run.has_value()) {
      html::translate_element(child, state);
      continue;
    }
    const Paragraph paragraph = child.as_paragraph();

    state.out().write_element_begin(
        "x-p", html::HtmlElementOptions().set_inline(true).set_style(
                   "display:block;" +
                       html::translate_paragraph_style(paragraph.style(),
                                                       state.direction()) +
                       html::translate_text_style(run_style(paragraph, *run)),
                   state.styles()));
    state.out().out() << xml::escape_text(run->content());
    html::write_paragraph_line_box(false, state);
    state.out().write_element_end("x-p");
  }
}

/// `formula::Settings` as the page's evaluator reads them
/// (`wasm/formula/odr_formula.cpp`).
std::string formula_settings_json(const formula::Settings &settings) {
  nlohmann::json result{
      {"dialect", settings.dialect == formula::Dialect::libreoffice
                      ? "libreoffice"
                      : "excel"},
      {"caseSensitive", settings.case_sensitive},
      {"wildcards", settings.wildcards},
      {"regularExpressions", settings.regular_expressions},
      {"wholeCell", settings.whole_cell},
      {"epoch",
       settings.epoch == number_format::Epoch::from_1904 ? 1904 : 1900},
      {"nullDate", settings.null_date},
      {"nullYear", settings.null_year},
  };
  if (settings.boolean_word.has_value()) {
    result["booleanWord"] = *settings.boolean_word;
  }
  if (settings.iteration.has_value()) {
    result["iteration"] = {{"steps", settings.iteration->steps},
                           {"difference", settings.iteration->difference}};
  }
  return result.dump();
}

/// The names a formula in the page reads, as the evaluator's `name` callback
/// gives them: `sheet` is null for a name of the whole document.
std::string formula_names_json(const std::span<const formula::Name> names) {
  nlohmann::json result = nlohmann::json::array();
  for (const formula::Name &name : names) {
    result.push_back({
        {"name", name.name},
        {"sheet", name.sheet.has_value() ? nlohmann::json(*name.sheet)
                                         : nlohmann::json()},
        {"expression", name.expression},
    });
  }
  return result.dump();
}

} // namespace

std::optional<HtmlSheetCut> html::sheet_cut(const Sheet &sheet,
                                            const HtmlConfig &config) {
  const TableDimensions rendered = sheet_rendered_extent(sheet, config);
  // Against the whole sheet, not the window: what dropping the limits would
  // render.
  const TableDimensions content = config.spreadsheet_limit_by_content
                                      ? sheet.content(std::nullopt)
                                      : sheet.dimensions();

  if (rendered.rows >= content.rows && rendered.columns >= content.columns) {
    return {};
  }
  return HtmlSheetCut{content, rendered};
}

namespace html {
namespace {

/// What every row of a sheet is written with, the same for the page and for
/// a chunk the page loads.
struct SheetRows final {
  Sheet sheet;
  std::uint32_t end_column{0};
  std::uint32_t end_row{0};
  std::vector<std::optional<double>> column_pixels{};
  std::vector<std::string> column_styles{};
  bool has_shapes{false};
  /// scaffolding: a read-only render states neither a lock nor a dependency
  std::optional<formula::Syntax> syntax{};
  std::uint32_t ordinal{0};
  std::unordered_map<std::string, std::uint32_t> ordinals_by_name{};
};

SheetRows sheet_rows(const Sheet &sheet, const WritingState &state) {
  SheetRows rows{.sheet = sheet};
  const TableDimensions rendered = sheet_rendered_extent(sheet, state.config());
  rows.end_column = rendered.columns;
  rows.end_row = rendered.rows;
  rows.syntax = state.config().editable ? state.formula_syntax() : std::nullopt;
  rows.ordinal = sheet_ordinal(sheet);
  if (rows.syntax.has_value()) {
    rows.ordinals_by_name = sheet_ordinals_by_name(sheet);
  }
  const ElementRange shapes = sheet.shapes();
  rows.has_shapes = shapes.begin() != shapes.end();

  // `table-layout:fixed` still sizes the table from its content, so an
  // unbroken line would widen its column; `max-width:0` takes the cell out of
  // that sum. Every chunk is a table of its own, so every column needs a
  // width: one that states none takes its longest text, as LibreOffice's
  // optimal width does.
  rows.column_pixels.resize(rows.end_column);
  rows.column_styles.resize(rows.end_column);
  for (std::uint32_t column_index = 0; column_index < rows.end_column;
       ++column_index) {
    const TableColumnStyle table_column_style =
        sheet.column_style(column_index);
    rows.column_pixels[column_index] = css_pixels(table_column_style.width);
    rows.column_styles[column_index] =
        translate_table_column_style(table_column_style);
  }
  for (std::uint32_t column_index = 0; column_index < rows.end_column;
       ++column_index) {
    if (rows.column_pixels[column_index].has_value()) {
      continue;
    }
    std::size_t characters = 0;
    for (std::uint32_t row_index = 0;
         row_index < std::min(rows.end_row, estimated_rows); ++row_index) {
      const SheetCell cell = sheet.cell(column_index, row_index);
      if (cell.is_covered()) {
        continue;
      }
      const std::string text = shown_text(cell);
      // a cell that wraps its text needs only its longest word
      const char *breaks =
          sheet.cell_style(column_index, row_index).wrap_text.value_or(false)
              ? "\n "
              : "\n";
      for (std::size_t begin = 0; begin <= text.size();) {
        const std::size_t end =
            std::min(text.find_first_of(breaks, begin), text.size());
        characters = std::max(characters, util::string::utf8_length(
                                              text.substr(begin, end - begin)));
        begin = end + 1;
      }
    }
    const double pixels =
        static_cast<double>(std::clamp<std::size_t>(characters, 4, 60)) *
            character_pixels +
        cell_padding_pixels;
    rows.column_pixels[column_index] = pixels;
    rows.column_styles[column_index] +=
        "width:" + util::number::to_string_significant(pixels, 7) + "px;";
  }
  return rows;
}

void write_columns(const SheetRows &rows, const WritingState &state) {
  state.out().write_element_begin("col",
                                  HtmlElementOptions()
                                      .set_close_type(HtmlCloseType::none)
                                      .set_class("odr-sheet-gutter"));
  for (const std::string &style : rows.column_styles) {
    state.out().write_element_begin("col",
                                    HtmlElementOptions()
                                        .set_close_type(HtmlCloseType::none)
                                        .set_style(style, state.styles()));
  }
}

/// Where the rows have got to: the cursor, the row per column where a cell
/// spanning down from above ends, and the cells an array formula fills, as
/// `row << 32 | column`, from its own cell on, which comes first.
struct RowCursor final {
  explicit RowCursor(const std::uint32_t end_column)
      : span_end_row(end_column, 0) {}

  TableCursor cursor;
  std::vector<std::uint32_t> span_end_row;
  std::unordered_set<std::uint64_t> array_cells;

  /// Whether a merge from above reaches into the row the cursor is at.
  [[nodiscard]] bool merge_open() const {
    return std::ranges::any_of(span_end_row, [&](const std::uint32_t end) {
      return end > cursor.row();
    });
  }
};

/// Row @p row_index of @p rows, and @p at moved past it.
void write_row(const SheetRows &rows, RowCursor &at, const WritingState &state,
               const std::uint32_t row_index) {
  const Sheet &sheet = rows.sheet;
  const std::uint32_t end_column = rows.end_column;
  const TableRowStyle table_row_style = sheet.row_style(row_index);

  state.out().write_element_begin(
      "tr", HtmlElementOptions().set_style(
                translate_table_row_style(table_row_style), state.styles()));

  state.out().write_element_begin(
      "th", HtmlElementOptions()
                .set_inline(true)
                .set_class("odr-sheet-row-header")
                .set_style(
                    [&]() -> std::string {
                      const std::optional<Measure> height =
                          table_row_style.height;
                      if (!height.has_value()) {
                        return {};
                      }
                      return "height:" + css_length(*height) +
                             ";max-height:" + css_length(*height) + ";";
                    }(),
                    state.styles()));
  state.out().write_raw(TablePosition::to_row_string(row_index));
  state.out().write_element_end("th");

  // Carried forward, so no position is read twice.
  std::optional<SheetCell> pending;
  for (std::uint32_t column_index = at.cursor.column();
       column_index < end_column; column_index = at.cursor.column()) {
    const SheetCell cell =
        pending.has_value() ? *pending : sheet.cell(column_index, row_index);
    pending.reset();

    if (cell.is_covered()) {
      // normally unreachable: the cursor skips positions covered by an
      // anchor's span; advance one column so inconsistent spans cannot
      // starve the loop
      at.cursor.add_cell();
      continue;
    }

    // TODO looks a bit odd to query the same (col, row) all the time. maybe
    // there could be a struct to get all the info?
    const TableCellStyle cell_style = sheet.cell_style(column_index, row_index);
    const TableDimensions cell_span = cell.span();
    const ValueType cell_value_type = cell.value_type();

    // `style:wrap-option` is `no-wrap` by default, and `wrapText` is off.
    const bool wraps = cell_style.wrap_text.value_or(false);
    const std::uint32_t next_column = column_index + cell_span.columns;
    std::optional<SheetCell> next;
    if (next_column < end_column) {
      next = sheet.cell(next_column, row_index);
    }

    const bool anchors_shapes =
        rows.has_shapes && column_index == 0 && row_index == 0;
    const bool cuts_its_text = !anchors_shapes && holds_only_text(cell);

    std::string cell_css;
    if (!wraps && cuts_its_text) {
      cell_css += "white-space:nowrap;";
    }
    if (wraps && cuts_its_text) {
      // Its block is held at the row's height, so a broken line runs past
      // the bottom and would paint over the row below.
      cell_css += "overflow:hidden;";
    }
    // Only nonblank cells spill, so each blank span is scanned once.
    if (!wraps && cuts_its_text &&
        rows.column_pixels[column_index].has_value() && !is_blank(cell)) {
      std::optional<double> spill(0);
      bool bounded = false;
      for (std::uint32_t ahead = next_column; ahead < end_column; ++ahead) {
        const SheetCell cell_ahead = ahead == next_column && next.has_value()
                                         ? *next
                                         : sheet.cell(ahead, row_index);
        // The merged cell from a row above paints over a covered position,
        // which a reader may give as a blank cell.
        if (at.span_end_row[ahead] > row_index || !is_blank(cell_ahead)) {
          bounded = true;
          break;
        }
        if (!rows.column_pixels[ahead].has_value()) {
          spill.reset();
          bounded = true;
          break;
        }
        *spill += *rows.column_pixels[ahead];
      }
      if (bounded) {
        if (!spill.has_value() || *spill == 0) {
          cell_css += "overflow:hidden;";
        } else {
          cell_css += "clip-path:inset(0 " +
                      util::number::to_string_significant(-*spill, 7) +
                      "px 0 0);";
        }
      }
    }

    const std::optional<FoldedCell> folded =
        fold_cell(cell, state, wraps, anchors_shapes, table_row_style.height);

    const CellValue cell_value =
        state.config().editable ? cell.value() : CellValue();
    // a formatted cell's text need not state the number a sort needs
    const std::optional<std::string> value_spelling =
        state.config().editable ? formatted_value(cell_value, cell)
        : cell_value_type == ValueType::date ||
                cell_value_type == ValueType::time ||
                cell_value_type == ValueType::float_number
            ? formatted_value(cell.value(), cell)
            : std::nullopt;
    const std::optional<TableDimensions> array =
        state.config().editable ? cell.array_span() : std::nullopt;
    if (array.has_value()) {
      for (std::uint32_t r = 0; r < array->rows; ++r) {
        for (std::uint32_t c = 0; c < array->columns; ++c) {
          at.array_cells.insert(std::uint64_t{row_index + r} << 32 |
                                (column_index + c));
        }
      }
    }
    // the page edits no part of an array formula, as Excel does not
    const char *lock =
        !state.config().editable ? nullptr
        : at.array_cells.contains(std::uint64_t{row_index} << 32 | column_index)
            ? "array"
            : cell_lock(cell_value, cell, anchors_shapes);
    // what a formula bar shows, and the rectangles the page marks against
    const bool computes = cell_value.has_formula() &&
                          !cell_value.formula().empty() &&
                          rows.syntax.has_value();
    const std::string reads =
        computes ? cell_reads(cell_value.formula(), *rows.syntax, rows.ordinal,
                              rows.ordinals_by_name, state.formula_names())
                 : std::string();
    // what a formula in the page reads the cell as, where the text is not it
    const char *read_type = !state.config().editable                ? nullptr
                            : cell_value_type == ValueType::boolean ? "boolean"
                            : cell_value_type == ValueType::error   ? "error"
                                                                    : nullptr;
    // the format a formula typed into the cell shows its result in
    const std::optional<std::string> number_format =
        rows.syntax.has_value()
            ? sheet.cell_number_format(column_index, row_index)
            : std::nullopt;

    state.out().write_element_begin(
        "td",
        HtmlElementOptions()
            // a cell preserves its white space, so nothing is indented in it
            .set_inline(true)
            .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
              if (cell_span.columns > 1) {
                clb("colspan", std::to_string(cell_span.columns));
              }
              if (cell_span.rows > 1) {
                clb("rowspan", std::to_string(cell_span.rows));
              }
              if (lock != nullptr) {
                clb("data-odr-lock", lock);
              }
              if (computes) {
                clb("data-odr-formula",
                    xml::escape_attribute(cell_value.formula()));
              }
              if (!reads.empty()) {
                clb("data-odr-reads", reads);
              }
              if (array.has_value() && computes) {
                clb("data-odr-array", std::to_string(array->columns) + "," +
                                          std::to_string(array->rows));
              }
              if (value_spelling.has_value()) {
                clb("data-odr-value", *value_spelling);
              }
              if (read_type != nullptr) {
                clb("data-odr-type", read_type);
                if (cell_value_type == ValueType::boolean &&
                    cell_value.has_number()) {
                  clb("data-odr-value", cell_value.number() != 0 ? "1" : "0");
                }
              }
              if (number_format.has_value()) {
                clb("data-odr-format", xml::escape_attribute(*number_format));
              }
            })
            .set_style(translate_table_cell_style(
                           cell_style, writes_dark_style(state.config())) +
                           (rows.column_pixels[column_index].has_value()
                                ? "max-width:0;"
                                : "") +
                           cell_css +
                           (folded.has_value() ? folded->style : std::string()),
                       state.styles())
            .set_class([&]() -> std::optional<HtmlWritable> {
              std::string clazz =
                  cell_value_type == ValueType::float_number
                      ? "odr-value-type-float"
                  : cell_value_type == ValueType::date ? "odr-value-type-date"
                  : cell_value_type == ValueType::time ? "odr-value-type-time"
                                                       : "";
              if (lock != nullptr) {
                clazz += clazz.empty() ? "odr-locked" : " odr-locked";
              }
              return clazz.empty() ? std::nullopt
                                   : std::optional<HtmlWritable>(clazz);
            }()));
    if (column_index == 0 && row_index == 0) {
      for (const Element shape : sheet.shapes()) {
        translate_element(shape, state);
      }
    }
    if (folded.has_value()) {
      state.out().out() << folded->text;
    } else {
      translate_cell_children(cell, state);
    }
    state.out().write_element_end("td");

    for (std::uint32_t column = column_index;
         column < std::min(next_column, end_column); ++column) {
      at.span_end_row[column] = row_index + cell_span.rows;
    }
    at.cursor.add_cell(cell_span.columns, cell_span.rows);
    if (at.cursor.column() == next_column) {
      pending = next;
    }
  }

  state.out().write_element_end("tr");

  at.cursor.add_row();
}

/// The `col`s and the `tbody` of rows [@p first_row, @p end_row), which no
/// merge from above reaches into, with their styles inline.
std::string sheet_chunk(const SheetRows &rows, const HtmlConfig &config,
                        const std::uint32_t first_row,
                        const std::uint32_t end_row) {
  std::ostringstream html;
  HtmlWriter out(html, false, "");
  HtmlResources resources;
  const Logger logger = Logger::null();
  WritingState state(out, config, resources, logger);
  state.set_editable_markup(false);

  write_columns(rows, state);
  out.write_element_begin("tbody");
  RowCursor at(rows.end_column);
  at.cursor.add_row(first_row);
  for (std::uint32_t row_index = first_row; row_index < end_row;
       row_index = at.cursor.row()) {
    write_row(rows, at, state, row_index);
  }
  out.write_element_end("tbody");
  return html.str();
}

/// A chunk of a sheet as the script that puts its rows into the page
/// (`lazy-loading.md` decision 2), rendered when it is asked for.
class SheetChunkResource final : public abstract::HtmlResource {
public:
  SheetChunkResource(std::shared_ptr<const SheetRows> rows,
                     std::shared_ptr<const HtmlConfig> config, std::string key,
                     std::string path, const std::uint32_t first_row,
                     const std::uint32_t end_row)
      : m_rows{std::move(rows)}, m_config{std::move(config)},
        m_key{std::move(key)}, m_path{std::move(path)}, m_first_row{first_row},
        m_end_row{end_row} {}

  [[nodiscard]] HtmlResourceType type() const override {
    return HtmlResourceType::js;
  }
  [[nodiscard]] const std::string &mime_type() const override {
    static const std::string mime_type = "text/javascript";
    return mime_type;
  }
  [[nodiscard]] const std::string &name() const override { return m_path; }
  [[nodiscard]] const std::string &path() const override { return m_path; }
  [[nodiscard]] const std::optional<File> &file() const override {
    static const std::optional<File> none;
    return none;
  }
  [[nodiscard]] bool is_shipped() const override { return false; }
  [[nodiscard]] bool is_external() const override { return false; }
  [[nodiscard]] bool is_accessible() const override { return true; }

  void write_resource(std::ostream &os) const override {
    os << "odr.chunks.fill(" << script_string(m_key) << ","
       << script_string(sheet_chunk(*m_rows, *m_config, m_first_row, m_end_row))
       << ");\n";
  }

private:
  std::shared_ptr<const SheetRows> m_rows;
  std::shared_ptr<const HtmlConfig> m_config;
  std::string m_key;
  std::string m_path;
  std::uint32_t m_first_row{0};
  std::uint32_t m_end_row{0};
};

/// The text under an element as `search.js` reads it from the DOM: the runs,
/// with a `\n` where the block they flow in changes. A paragraph, a list, a
/// table and a frame are each a block, and a span or a link is not.
struct BlockText final {
  std::string text;
  /// the block the last run flowed in, 0 before the first
  std::uint64_t owner{0};
  std::uint64_t blocks{0};

  void append(const Element &element, const std::uint64_t in) {
    for (const Element child : element.children()) {
      switch (child.type()) {
      case ElementType::text: {
        std::string content = child.as_text().content();
        if (content.empty()) {
          break;
        }
        // `search.js` folds it to a space; only a new block writes a `\n`
        std::ranges::replace(content, '\n', ' ');
        if (owner != 0 && owner != in) {
          text += '\n';
        }
        owner = in;
        text += content;
        break;
      }
      case ElementType::span:
      case ElementType::link:
      case ElementType::bookmark:
      case ElementType::line_break:
        append(child, in);
        break;
      default:
        append(child, ++blocks);
        break;
      }
    }
  }
};

/// The text of rows [@p first_row, @p end_row): a block for each cell, and for
/// each paragraph in it. The headers are not in it, because a search skips
/// them.
std::string sheet_chunk_text(const SheetRows &rows,
                             const std::uint32_t first_row,
                             const std::uint32_t end_row) {
  BlockText text;
  for (std::uint32_t row_index = first_row; row_index < end_row; ++row_index) {
    for (std::uint32_t column_index = 0; column_index < rows.end_column;
         ++column_index) {
      const SheetCell cell = rows.sheet.cell(column_index, row_index);
      if (!cell.is_covered()) {
        text.append(cell, ++text.blocks);
      }
    }
  }
  return std::move(text.text);
}

/// A chunk whose text a script of the search index holds.
struct IndexedChunk final {
  std::string key;
  std::uint32_t first_row{0};
  std::uint32_t end_row{0};
};

/// A script of the search index of a sheet, rendered when it is asked for.
/// The writer adds each chunk once it knows where the chunk ends.
class SheetIndexResource final : public abstract::HtmlResource {
public:
  SheetIndexResource(std::shared_ptr<const SheetRows> rows, std::string path,
                     std::shared_ptr<const std::vector<IndexedChunk>> chunks)
      : m_rows{std::move(rows)}, m_path{std::move(path)},
        m_chunks{std::move(chunks)} {}

  [[nodiscard]] HtmlResourceType type() const override {
    return HtmlResourceType::js;
  }
  [[nodiscard]] const std::string &mime_type() const override {
    static const std::string mime_type = "text/javascript";
    return mime_type;
  }
  [[nodiscard]] const std::string &name() const override { return m_path; }
  [[nodiscard]] const std::string &path() const override { return m_path; }
  [[nodiscard]] const std::optional<File> &file() const override {
    static const std::optional<File> none;
    return none;
  }
  [[nodiscard]] bool is_shipped() const override { return false; }
  [[nodiscard]] bool is_external() const override { return false; }
  [[nodiscard]] bool is_accessible() const override { return true; }

  void write_resource(std::ostream &os) const override {
    os << "odr.chunks.text({";
    for (const IndexedChunk &chunk : *m_chunks) {
      os << (&chunk == m_chunks->data() ? "" : ",") << script_string(chunk.key)
         << ":"
         << script_string(
                sheet_chunk_text(*m_rows, chunk.first_row, chunk.end_row));
    }
    os << "});\n";
  }

private:
  std::shared_ptr<const SheetRows> m_rows;
  std::string m_path;
  std::shared_ptr<const std::vector<IndexedChunk>> m_chunks;
};

} // namespace
} // namespace html

void html::translate_sheet(const Sheet &sheet, const WritingState &state) {
  // a sheet's editing is an overlay, so its content carries none
  WritingState sheet_state = state;
  sheet_state.set_editable_markup(false);

  const auto rows =
      std::make_shared<const SheetRows>(sheet_rows(sheet, sheet_state));
  const std::uint32_t end_column = rows->end_column;
  const std::uint32_t end_row = rows->end_row;

  const std::optional<double> print_fit = sheet_print_fit(sheet, end_column);
  const std::optional<HtmlSheetCut> cut =
      state.config().editable ? sheet_cut(sheet, state.config()) : std::nullopt;

  state.out().write_element_begin(
      "div",
      HtmlElementOptions()
          .set_class("odr-sheet")
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            // every op names its sheet, and a view holds only one
            clb("data-odr-sheet", std::to_string(rows->ordinal));
            // the name a formula reads this sheet by
            if (rows->syntax.has_value()) {
              clb("data-odr-sheet-name", xml::escape_attribute(sheet.name()));
            }
            if (cut.has_value()) {
              clb("data-odr-cut", std::to_string(cut->content.columns) + "," +
                                      std::to_string(cut->content.rows));
            }
          })
          .set_style([&]() -> std::string {
            // Every table states the same width, so `table-layout:fixed`
            // takes the columns from the `col`s alone. A table wider than the
            // viewport would else shrink a column to what its cells hold.
            const double gutter =
                static_cast<double>(
                    TablePosition::to_row_string(end_row == 0 ? 0 : end_row - 1)
                        .size()) *
                    digit_pixels +
                gutter_padding_pixels;
            double width = gutter;
            for (const std::optional<double> &pixels : rows->column_pixels) {
              width += pixels.value_or(0);
            }
            std::string style = "--odr-sheet-gutter:" +
                                util::number::to_string_significant(gutter, 7) +
                                "px;--odr-sheet-width:" +
                                util::number::to_string_significant(width, 7) +
                                "px;";
            if (print_fit.has_value()) {
              // `Measure` renders no exponent form
              style += "--odr-print-fit:" +
                       Measure(*print_fit, DynamicUnit()).to_string() + ";";
            }
            return style;
          }()));

  // No `scope`: the letters and numbers are a ruler, not headers of what they
  // label. A table of its own, because a sticky `thead` sticks only inside
  // its table, and the rows are in many.
  {
    state.out().write_element_begin(
        "table", HtmlElementOptions().set_class("odr-sheet-head"));
    write_columns(*rows, state);
    state.out().write_element_begin("thead");
    state.out().write_element_begin("tr");

    state.out().write_element_begin(
        "th",
        HtmlElementOptions().set_inline(true).set_class("odr-sheet-corner"));
    state.out().write_element_end("th");

    for (std::uint32_t column_index = 0; column_index < end_column;
         ++column_index) {
      state.out().write_element_begin(
          "th", HtmlElementOptions().set_inline(true).set_class(
                    "odr-sheet-column-header"));
      state.out().write_raw(TablePosition::to_column_string(column_index));
      state.out().write_element_end("th");
    }

    state.out().write_element_end("tr");
    state.out().write_element_end("thead");
    state.out().write_element_end("table");
  }

  // a chunk ends where no merge is open, so no `rowspan` crosses two tables;
  // without lazy layout, all the rows are one table
  const bool lazy = state.config().lazy_layout;
  // a chunk after the first loads its rows once it is near; they are written
  // here too, to nothing, because that is where the chunk is found to end,
  // and the page still lists their resources, which `bring_offline` writes
  const bool loads =
      lazy && state.config().lazy_loading && !state.config().editable;
  // one copy for all the chunks of the sheet
  const std::shared_ptr<const HtmlConfig> chunk_config =
      loads ? std::make_shared<const HtmlConfig>(state.config()) : nullptr;
  NullStream null;
  HtmlWriter null_out(null, false, "");
  WritingState null_state(null_out, state.config(), state.resources(),
                          state.logger());
  null_state.set_editable_markup(false);

  const std::uint32_t chunk_rows =
      lazy ? std::clamp<std::uint32_t>(
                 chunk_cells / std::max<std::uint32_t>(end_column, 1), 8, 256)
           : std::max<std::uint32_t>(end_row, 1);
  std::uint32_t chunk_index = 0;
  std::uint32_t chunk_first_row = 0;
  std::uint32_t rows_in_chunk = 0;
  bool chunk_open = false;
  bool placeholder = false;
  // the chunks of the group of the search index the open chunk is in
  std::shared_ptr<std::vector<IndexedChunk>> indexed;
  std::string index_path;
  const auto chunk_key = [&] {
    return std::to_string(rows->ordinal) + "-" + std::to_string(chunk_index);
  };
  const auto chunk_path = [&] {
    return "sheet" + std::to_string(rows->ordinal) + "-chunks/" +
           std::to_string(chunk_index) + ".js";
  };
  const auto begin_chunk = [&](const std::uint32_t first_row) {
    placeholder = loads && first_row > 0;
    // the first chunk of a group that loads names the script of its text
    const bool begins_index =
        placeholder &&
        (chunk_index == 1 || chunk_index % chunks_per_index == 0);
    if (begins_index) {
      indexed = std::make_shared<std::vector<IndexedChunk>>();
      index_path = "sheet" + std::to_string(rows->ordinal) + "-search/" +
                   std::to_string(chunk_index / chunks_per_index) + ".js";
      state.resources().emplace_back(
          odr::HtmlResource(
              std::make_shared<SheetIndexResource>(rows, index_path, indexed)),
          index_path);
    }
    double height = 0;
    for (std::uint32_t row_index = first_row;
         lazy && row_index < std::min(end_row, first_row + chunk_rows);
         ++row_index) {
      height += css_pixels(sheet.row_style(row_index).height)
                    .value_or(estimated_row_pixels);
    }
    const std::string pixels = util::number::to_string_significant(height, 7);
    state.out().write_element_begin(
        "table",
        HtmlElementOptions()
            .set_class("odr-sheet-rows" +
                       chunk_classes(state.config(), first_row == 0))
            .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
              // a shape hangs from A1 and may reach past the first chunk
              if (lazy && first_row == 0 && rows->has_shapes) {
                clb("data-odr-chunk-held", "");
              }
              if (placeholder) {
                clb("data-odr-chunk", chunk_key());
                clb("data-odr-chunk-src", chunk_path());
              }
              if (begins_index) {
                clb("data-odr-chunk-text", index_path);
              }
            })
            // a browser before Chrome 95 drops the `auto` declaration and
            // keeps only the size
            .set_style(
                lazy ? std::optional<HtmlWritable>(
                           "contain-intrinsic-size:0 " + pixels +
                           "px;contain-intrinsic-height:auto " + pixels + "px" +
                           (placeholder ? ";--odr-chunk-height:" + pixels + "px"
                                        : ""))
                     : std::nullopt));
    write_columns(*rows, state);
    state.out().write_element_begin("tbody");
    chunk_open = true;
    chunk_first_row = first_row;
    rows_in_chunk = 0;
  };
  const auto end_chunk = [&](const std::uint32_t end) {
    state.out().write_element_end("tbody");
    state.out().write_element_end("table");
    if (placeholder) {
      state.resources().emplace_back(
          odr::HtmlResource(std::make_shared<SheetChunkResource>(
              rows, chunk_config, chunk_key(), chunk_path(), chunk_first_row,
              end)),
          chunk_path());
      indexed->push_back(IndexedChunk{chunk_key(), chunk_first_row, end});
    }
    chunk_open = false;
    ++chunk_index;
  };

  RowCursor at(end_column);
  for (std::uint32_t row_index = at.cursor.row(); row_index < end_row;
       row_index = at.cursor.row()) {
    if (!chunk_open) {
      begin_chunk(row_index);
    }
    write_row(*rows, at, placeholder ? null_state : sheet_state, row_index);
    if (++rows_in_chunk >= chunk_rows && !at.merge_open()) {
      end_chunk(at.cursor.row());
    }
  }
  if (chunk_open) {
    end_chunk(at.cursor.row());
  }

  state.out().write_element_end("div");
}

std::string html::sheet_body_classes(const HtmlConfig &config) {
  switch (config.spreadsheet_gridlines) {
  case HtmlTableGridlines::soft:
    return " odr-gridlines-soft";
  case HtmlTableGridlines::hard:
    return " odr-gridlines-hard";
  case HtmlTableGridlines::none:
  default:
    return " odr-gridlines-none";
  }
}

void html::write_sheet_body_attributes(const Document &document,
                                       const WritingState &state,
                                       const HtmlAttributeWriterCallback &clb) {
  if (const std::optional<bool> on_click = state.config().sheet_edit_on_click;
      on_click.has_value()) {
    clb("data-odr-sheet-edit-on-click", *on_click ? "true" : "false");
  }
  if (document.file_type() == FileType::comma_separated_values) {
    clb("data-odr-sheet-styles", "false");
  }
  // the decimal separator a typed number is read with
  if (const std::optional<std::string> locale = document.locale();
      locale.has_value()) {
    clb("data-odr-locale", xml::escape_attribute(*locale));
  }
  // what the page computes a formula cell with
  if (const std::optional<formula::Syntax> syntax = state.formula_syntax();
      syntax.has_value()) {
    clb("data-odr-formula-syntax",
        *syntax == formula::Syntax::opendocument ? "opendocument" : "ooxml");
    clb("data-odr-formula-settings",
        xml::escape_attribute(
            formula_settings_json(document.impl()->formula_settings())));
    if (const std::vector<formula::Name> *names = state.formula_names();
        names != nullptr && !names->empty()) {
      clb("data-odr-formula-names",
          xml::escape_attribute(formula_names_json(*names)));
    }
  }
}

void html::write_sheet_scripts(const WritingState &state) {
  write_spreadsheet_script(state);
  if (state.config().editable) {
    // a sheet without a formula takes one as typed input
    if (state.document_editable() && state.formula_syntax().has_value()) {
      write_formula_script(state);
    }
    write_sheet_editing_script(state);
  }
}

} // namespace odr::internal
