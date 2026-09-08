#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/html/style_registry.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <vector>

namespace odr::internal {

namespace {

/// Names every enumerator, with no `default`, so a type added to
/// @ref ElementType has to be named here too.
const char *element_type_name(const ElementType type) {
  switch (type) {
  case ElementType::none:
    return "none";
  case ElementType::root:
    return "root";
  case ElementType::slide:
    return "slide";
  case ElementType::sheet:
    return "sheet";
  case ElementType::page:
    return "page";
  case ElementType::master_page:
    return "master_page";
  case ElementType::sheet_cell:
    return "sheet_cell";
  case ElementType::text:
    return "text";
  case ElementType::line_break:
    return "line_break";
  case ElementType::page_break:
    return "page_break";
  case ElementType::paragraph:
    return "paragraph";
  case ElementType::span:
    return "span";
  case ElementType::link:
    return "link";
  case ElementType::bookmark:
    return "bookmark";
  case ElementType::list:
    return "list";
  case ElementType::list_item:
    return "list_item";
  case ElementType::table:
    return "table";
  case ElementType::table_column:
    return "table_column";
  case ElementType::table_row:
    return "table_row";
  case ElementType::table_cell:
    return "table_cell";
  case ElementType::frame:
    return "frame";
  case ElementType::image:
    return "image";
  case ElementType::group:
    return "group";
  }
  return "?";
}

} // namespace

void html::translate_children(const ElementRange &range,
                              const WritingState &state) {
  for (const Element child : range) {
    translate_element(child, state);
  }
}

void html::translate_element(const Element &element,
                             const WritingState &state) {
  switch (element.type()) {
  case ElementType::text:
    translate_text(element, state);
    break;
  case ElementType::line_break:
    translate_line_break(element, state);
    break;
  case ElementType::paragraph:
    translate_paragraph(element, state);
    break;
  case ElementType::span:
    translate_span(element, state);
    break;
  case ElementType::link:
    translate_link(element, state);
    break;
  case ElementType::bookmark:
    translate_bookmark(element, state);
    break;
  case ElementType::list:
    translate_list(element, state);
    break;
  case ElementType::list_item:
    translate_list_item(element, state);
    break;
  case ElementType::table:
    translate_table(element, state);
    break;
  case ElementType::frame:
    translate_frame(element, state);
    break;
  case ElementType::image:
    translate_image(element, state);
    break;
  case ElementType::page_break:
    translate_page_break(element, state);
    break;
  case ElementType::group:
    translate_children(element.children(), state);
    break;
  default:
    // Dropped with its whole subtree; a renderer shows what it can.
    ODR_WARNING(state.logger(), "html: dropped unhandled element "
                                    << element_type_name(element.type()));
    break;
  }
}

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
  if (config.spreadsheet_limit) {
    end_column = std::min(end_column, config.spreadsheet_limit->columns);
    end_row = std::min(end_row, config.spreadsheet_limit->rows);
  }
  end_column = std::max(1u, end_column);
  if (config.spreadsheet_cell_limit) {
    const std::uint64_t rows = *config.spreadsheet_cell_limit / end_column;
    end_row = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(end_row, std::max<std::uint64_t>(1, rows)));
  }
  end_row = std::max(1u, end_row);

  return {end_row, end_column};
}

namespace {

/// Whether a reader sees anything. A bookmark marks a place rather than filling
/// one, and a span or a link is a style around what it holds, so a paragraph
/// holding only those is still an empty line.
bool has_content(const ElementRange &children) {
  for (const Element child : children) {
    switch (child.type()) {
    case ElementType::bookmark:
      break;
    case ElementType::span:
    case ElementType::link:
      if (has_content(child.children())) {
        return true;
      }
      break;
    case ElementType::text:
      if (!child.as_text().content().empty()) {
        return true;
      }
      break;
    default:
      return true;
    }
  }
  return false;
}

/// A break where the paragraph holds nothing, so a blank line survives being
/// pasted elsewhere; otherwise a break opportunity, or content all out of flow
/// leaves no line box.
void write_paragraph_line_box(const bool empty,
                              const html::WritingState &state) {
  state.out().write_element_begin(
      empty ? "br" : "wbr",
      html::HtmlElementOptions().set_close_type(html::HtmlCloseType::none));
}

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
/// raised run or an editable one: each means something else on the box.
/// Whether @p element carries `contenteditable`: an editable run of a view
/// that writes its editing into the markup.
bool writes_editable(const Element &element, const html::WritingState &state) {
  return state.editable_markup() && state.config().editable &&
         element.is_editable();
}

std::optional<Text> plain_text(const Element &element,
                               const html::WritingState &state) {
  if (element.type() != ElementType::text) {
    return {};
  }
  if (writes_editable(element, state)) {
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
        has_content(child.children())) {
      return false;
    }
  }
  return true;
}

/// Empty, or one text run at most - what a write can replace. odf wraps a
/// cell's text in a `text:p`, ooxml hangs it under the `c` directly, so a
/// single paragraph is unwrapped once.
bool holds_one_run(const ElementRange &children, const bool unwrap = true) {
  ElementIterator child = children.begin();
  if (child == children.end()) {
    return true;
  }
  const Element only = *child;
  if (++child != children.end()) {
    return false;
  }
  if (only.type() == ElementType::text) {
    return true;
  }
  return unwrap && only.type() == ElementType::paragraph &&
         holds_one_run(only.children(), false);
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

/// Why a cell cannot be edited, or null where it can be. The names the page
/// reports to its host; `spreadsheet-editing.md` decision 3 lists them.
const char *cell_lock(const SheetCell &cell, const bool anchors_shapes) {
  if (cell.value().has_formula()) {
    return "formula";
  }
  // its drawings are what the cell is, and an overlay would cover them
  if (anchors_shapes) {
    return "shapes";
  }
  // a write replaces the cell's one run, so anything richer would be lost
  return holds_one_run(cell.children()) ? nullptr : "rich";
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
                      html::escape_text(run->content())};
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
                    html::escape_text(run->content())};
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
    state.out().out() << html::escape_text(run->content());
    write_paragraph_line_box(false, state);
    state.out().write_element_end("x-p");
  }
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

void html::translate_sheet(const Sheet &sheet, const WritingState &state) {
  // a sheet's editing is an overlay, so its content carries none
  WritingState sheet_state = state;
  sheet_state.set_editable_markup(false);

  const TableDimensions rendered = sheet_rendered_extent(sheet, state.config());
  const std::uint32_t end_column = rendered.columns;
  const std::uint32_t end_row = rendered.rows;

  const std::optional<double> print_fit = sheet_print_fit(sheet, end_column);

  state.out().write_element_begin(
      "table",
      HtmlElementOptions()
          .set_class("odr-sheet")
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            // what the editor asks before the user clicks anything
            clb("data-odr-editable",
                state.document_editable() ? "true" : "readOnly");
            // every op names its sheet, and a view holds only one
            clb("data-odr-sheet", std::to_string(sheet_ordinal(sheet)));
          })
          .set_style([&]() -> std::optional<HtmlWritable> {
            if (!print_fit.has_value()) {
              return std::nullopt;
            }
            // `Measure` renders no exponent form
            return "--odr-print-fit:" +
                   Measure(*print_fit, DynamicUnit()).to_string() + ";";
          }()));

  state.out().write_element_begin("col",
                                  HtmlElementOptions()
                                      .set_close_type(HtmlCloseType::none)
                                      .set_class("odr-sheet-gutter"));

  // `table-layout:fixed` still sizes the table from its content, so an unbroken
  // line would widen its column; `max-width:0` takes the cell out of that sum.
  // Only where a width is stated: a column that states none is its content's.
  std::vector<std::optional<double>> column_pixels(end_column);

  for (std::uint32_t column_index = 0; column_index < end_column;
       ++column_index) {
    const TableColumnStyle table_column_style =
        sheet.column_style(column_index);
    column_pixels[column_index] = css_pixels(table_column_style.width);

    state.out().write_element_begin(
        "col", HtmlElementOptions()
                   .set_close_type(HtmlCloseType::none)
                   .set_style(translate_table_column_style(table_column_style),
                              state.styles()));
  }

  // No `scope`: the letters and numbers are a ruler, not headers of what they
  // label.
  {
    state.out().write_element_begin("thead");
    state.out().write_element_begin("tr");

    // Under `table-layout:fixed` the first row sizes the columns, and `ch`
    // resolves against the ruler's font — so the gutter width sits here rather
    // than on the `<col>`.
    state.out().write_element_begin(
        "th",
        HtmlElementOptions()
            .set_inline(true)
            .set_class("odr-sheet-corner")
            .set_style("width:calc(" +
                       std::to_string(
                           TablePosition::to_row_string(end_row - 1).size()) +
                       "ch + 14px);"));
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
  }

  state.out().write_element_begin("tbody");

  const ElementRange shapes = sheet.shapes();
  const bool has_shapes = shapes.begin() != shapes.end();

  TableCursor cursor;
  for (std::uint32_t row_index = cursor.row(); row_index < end_row;
       row_index = cursor.row()) {
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
                        return "height:" + height->to_string() +
                               ";max-height:" + height->to_string() + ";";
                      }(),
                      state.styles()));
    state.out().write_raw(TablePosition::to_row_string(row_index));
    state.out().write_element_end("th");

    // Carried forward, so no position is read twice.
    std::optional<SheetCell> pending;
    for (std::uint32_t column_index = cursor.column();
         column_index < end_column; column_index = cursor.column()) {
      const SheetCell cell =
          pending.has_value() ? *pending : sheet.cell(column_index, row_index);
      pending.reset();

      if (cell.is_covered()) {
        // normally unreachable: the cursor skips positions covered by an
        // anchor's span; advance one column so inconsistent spans cannot
        // starve the loop
        cursor.add_cell();
        continue;
      }

      // TODO looks a bit odd to query the same (col, row) all the time. maybe
      // there could be a struct to get all the info?
      const TableCellStyle cell_style =
          sheet.cell_style(column_index, row_index);
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
          has_shapes && column_index == 0 && row_index == 0;
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
      // Over the empty cells beside it, cut where the next one has something
      // to show, unbounded where nothing follows. Each blank cell is walked by
      // the one cell that may spill over it, so the row costs one pass.
      if (!wraps && cuts_its_text && column_pixels[column_index].has_value()) {
        std::optional<double> spill(0);
        bool bounded = false;
        for (std::uint32_t ahead = next_column; ahead < end_column; ++ahead) {
          const SheetCell cell_ahead = ahead == next_column && next.has_value()
                                           ? *next
                                           : sheet.cell(ahead, row_index);
          if (!is_blank(cell_ahead)) {
            bounded = true;
            break;
          }
          if (!column_pixels[ahead].has_value()) {
            spill.reset();
            bounded = true;
            break;
          }
          *spill += *column_pixels[ahead];
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

      const std::optional<FoldedCell> folded = fold_cell(
          cell, sheet_state, wraps, anchors_shapes, table_row_style.height);

      const char *lock = cell_lock(cell, anchors_shapes);

      state.out().write_element_begin(
          "td",
          HtmlElementOptions()
              .set_inline(folded.has_value())
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
              })
              .set_style(
                  translate_table_cell_style(cell_style) +
                      (column_pixels[column_index].has_value() ? "max-width:0;"
                                                               : "") +
                      cell_css +
                      (folded.has_value() ? folded->style : std::string()),
                  state.styles())
              .set_class([&]() -> std::optional<HtmlWritable> {
                const bool number = cell_value_type == ValueType::float_number;
                if (number && lock != nullptr) {
                  return "odr-value-type-float odr-locked";
                }
                if (number) {
                  return "odr-value-type-float";
                }
                return lock != nullptr
                           ? std::optional<HtmlWritable>("odr-locked")
                           : std::nullopt;
              }()));
      if (column_index == 0 && row_index == 0) {
        for (const Element shape : sheet.shapes()) {
          translate_element(shape, sheet_state);
        }
      }
      if (folded.has_value()) {
        state.out().out() << folded->text;
      } else {
        translate_cell_children(cell, sheet_state);
      }
      state.out().write_element_end("td");

      cursor.add_cell(cell_span.columns, cell_span.rows);
      if (cursor.column() == next_column) {
        pending = next;
      }
    }

    state.out().write_element_end("tr");

    cursor.add_row();
  }

  state.out().write_element_end("tbody");
  state.out().write_element_end("table");
}

namespace {

/// A slide or a drawing page: the master page's content under the page's own,
/// inside one outer page box. There is no inner (margin) box — unlike a text
/// document, both anchor their children absolutely at page coordinates.
template <typename PageLike>
void translate_page_like(const PageLike &page,
                         const html::WritingState &state) {
  state.out().write_element_begin(
      "div",
      html::HtmlElementOptions()
          .set_class("odr-page-outer")
          .set_style(html::translate_outer_page_style(page.page_layout())));

  html::translate_master_page(page.master_page(), state);
  html::translate_children(page.children(), state);

  state.out().write_element_end("div");
}

} // namespace

void html::translate_slide(const Slide &slide, const WritingState &state) {
  translate_page_like(slide, state);
}

void html::translate_page(const Page &page, const WritingState &state) {
  translate_page_like(page, state);
}

void html::translate_master_page(const MasterPage &masterPage,
                                 const WritingState &state) {
  for (const Element child : masterPage.children()) {
    // TODO filter placeholders
    translate_element(child, state);
  }
}

void html::translate_text(const Element &element, const WritingState &state) {
  const Text text = element.as_text();

  state.out().write_element_begin(
      "x-s",
      HtmlElementOptions()
          .set_inline(true)
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            if (writes_editable(element, state)) {
              clb("contenteditable", "true");
              clb("data-odr-path", element.document_path().to_string());
            }
          })
          .set_style(translate_text_style(text.style()), state.styles()));
  state.out().out() << escape_text(text.content());
  state.out().write_element_end("x-s");
}

void html::translate_line_break(const Element &element,
                                const WritingState &state) {
  const LineBreak line_break = element.as_line_break();

  state.out().write_element_begin(
      "br", HtmlElementOptions().set_close_type(HtmlCloseType::none));
  state.out().write_element_begin(
      "x-s", HtmlElementOptions().set_inline(true).set_style(
                 translate_text_style(line_break.style())));
  state.out().write_element_end("x-s");
}

namespace {} // namespace

void html::translate_page_break(const Element & /*element*/,
                                const WritingState &state) {
  // Reached only where the page box cannot be split; `TextHtmlFragment` takes
  // the breaks among the root's children.
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style("break-before:page"));
  state.out().write_element_end("div");
}

void html::translate_paragraph(const Element &element,
                               const WritingState &state,
                               const std::string &marker) {
  const Paragraph paragraph = element.as_paragraph();

  state.out().write_element_begin(
      "x-p",
      HtmlElementOptions().set_inline(true).set_style(
          "display:block;" +
              translate_paragraph_style(paragraph.style(), state.direction()) +
              translate_block_font_style(paragraph.text_style()),
          state.styles()));
  if (!marker.empty()) {
    state.out().write_element_begin(
        "x-s", HtmlElementOptions()
                   .set_inline(true)
                   .set_class("odr-list-marker")
                   .set_style(translate_text_style(paragraph.text_style())));
    // The tab separates label from text once copied.
    state.out().out() << escape_text(marker) << "&#9;";
    state.out().write_element_end("x-s");
  }
  translate_children(paragraph.children(), state);
  write_paragraph_line_box(marker.empty() && !has_content(paragraph.children()),
                           state);
  state.out().write_element_end("x-p");
}

void html::translate_span(const Element &element, const WritingState &state) {
  const Span span = element.as_span();

  state.out().write_element_begin(
      "x-s", HtmlElementOptions().set_inline(true).set_style(
                 translate_text_style(span.style()), state.styles()));
  translate_children(span.children(), state);
  state.out().write_element_end("x-s");
}

void html::translate_link(const Element &element, const WritingState &state) {
  const Link link = element.as_link();
  const std::string href = link.href();
  const UriKind kind = uri_kind(href);

  // A refused target loses the attribute, not the element.
  HtmlAttributesVector attributes;
  if (kind != UriKind::refused) {
    attributes.emplace_back("href", xml::escape_attribute(href));
  }

  HtmlElementOptions options =
      HtmlElementOptions().set_inline(true).set_attributes(
          std::move(attributes));
  if (const std::string_view target = link_target_attributes(kind);
      !target.empty()) {
    options.set_extra(std::string(target));
  }

  state.out().write_element_begin("a", options);
  translate_children(link.children(), state);
  state.out().write_element_end("a");
}

void html::translate_bookmark(const Element &element,
                              const WritingState &state) {
  const Bookmark bookmark = element.as_bookmark();

  state.out().write_element_begin(
      "a",
      HtmlElementOptions().set_inline(true).set_attributes(HtmlAttributesVector{
          {"id", xml::escape_attribute(bookmark.name())}}));
  state.out().write_element_end("a");
}

void html::translate_list(const Element &element, const WritingState &state) {
  // `div`s, not `ul`/`li`: an importer that draws its own marker over the one
  // we write shows both, and the macOS rich-text one does exactly that whatever
  // `list-style` says. The roles keep what a screen reader needs.
  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_class("odr-list")
                 .set_attributes(HtmlAttributesVector{{"role", "list"}}));
  translate_children(element.children(), state);
  state.out().write_element_end("div");
}

void html::translate_list_item(const Element &element,
                               const WritingState &state) {
  const ListItem list_item = element.as_list_item();

  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_class("odr-list-item")
                 .set_attributes(HtmlAttributesVector{{"role", "listitem"}})
                 .set_style(translate_text_style(list_item.style())));

  // Inside the first paragraph, not beside it: a sibling of that block copies
  // onto a line of its own.
  std::string marker = list_item.marker();
  for (const Element child : list_item.children()) {
    if (!marker.empty() && child.type() == ElementType::paragraph) {
      translate_paragraph(child, state, marker);
      marker.clear();
      continue;
    }
    translate_element(child, state);
  }

  state.out().write_element_end("div");
}

void html::translate_table(const Element &element, const WritingState &state) {
  const Table table = element.as_table();

  state.out().write_element_begin(
      "table",
      HtmlElementOptions()
          .set_attributes(HtmlAttributesVector{
              {"cellpadding", "0"}, {"border", "0"}, {"cellspacing", "0"}})
          .set_style(translate_table_style(table.style())));

  for (Element column : table.columns()) {
    TableColumn table_column = column.as_table_column();

    state.out().write_element_begin(
        "col",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::none)
            .set_style(translate_table_column_style(table_column.style())));
  }

  for (Element row : table.rows()) {
    TableRow table_row = row.as_table_row();

    state.out().write_element_begin(
        "tr", HtmlElementOptions().set_style(
                  translate_table_row_style(table_row.style())));

    for (Element cell : table_row.children()) {
      TableCell table_cell = cell.as_table_cell();

      if (table_cell.is_covered()) {
        continue;
      }

      TableDimensions cell_span = table_cell.span();

      state.out().write_element_begin(
          "td",
          HtmlElementOptions()
              .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                if (cell_span.columns > 1) {
                  clb("colspan", std::to_string(cell_span.columns));
                }
                if (cell_span.rows > 1) {
                  clb("rowspan", std::to_string(cell_span.rows));
                }
              })
              .set_style(translate_table_cell_style(table_cell.style())));

      translate_children(cell.children(), state);

      state.out().write_element_end("td");
    }

    state.out().write_element_end("tr");
  }

  state.out().write_element_end("table");
}

void html::translate_image(const Element &element, const WritingState &state) {
  const Image image = element.as_image();

  odr::HtmlResource resource;
  HtmlResourceLocation resource_location;
  if (image.is_internal()) {
    // the location is resolved against the document, so an engine naming the
    // image by its absolute path in the container has to lose the root
    const std::string path = Path(image.href()).make_relative().string();
    resource = HtmlResource::create(HtmlResourceType::image, "image/jpg", path,
                                    path, image.file(), false, false, true);
    resource_location =
        state.config().resource_locator(resource, state.config());
  } else {
    resource =
        HtmlResource::create(HtmlResourceType::image, "image/jpg", "image",
                             "image", std::nullopt, false, false, false);
    resource_location = image.href();
  }
  state.resources().emplace_back(std::move(resource), resource_location);

  state.out().write_element_begin(
      "img",
      HtmlElementOptions()
          .set_close_type(HtmlCloseType::trailing)
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            clb("alt", "Error: image not found or unsupported");
            if (resource_location.has_value()) {
              clb("src", xml::escape_attribute(resource_location.value()));
            } else {
              clb("src", [&](std::ostream &o) {
                // reached only for internal images, which have a file
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                translate_image_src(image.file().value(), o, state.config(),
                                    state.logger());
              });
            }
          })
          .set_style("position:absolute;left:0;top:0;width:100%;height:100%"));
}

namespace html {
namespace {

void translate_plain_frame(const Frame &frame, const GraphicStyle &style,
                           const WritingState &state) {
  // A plain frame is a box, so its fill has to be a background - the `fill`
  // that `translate_drawing_style` writes only reaches the svg a shape carries.
  std::string background;
  if (style.fill_color.has_value() && style.fill_color->alpha != 0) {
    background = "background-color:" + color(*style.fill_color) + ";";
  }
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_frame_properties(frame) +
                                            translate_drawing_style(style) +
                                            background));
  translate_children(frame.children(), state);
  state.out().write_element_end("div");
}

void translate_rect(const Frame &frame, const GraphicStyle &style,
                    const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_shape_properties(frame) +
                                            translate_drawing_style(style)));
  translate_children(frame.children(), state);
  state.out().write_new_line();
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)");
  state.out().write_element_end("div");
}

void translate_ellipse(const Frame &frame, const GraphicStyle &style,
                       const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_shape_properties(frame) +
                                            translate_drawing_style(style)));
  state.out().write_new_line();
  translate_children(frame.children(), state);
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><ellipse cx="50%" cy="50%" rx="50%" ry="50%" /></svg>)");
  state.out().write_element_end("div");
}

void translate_line(const Frame &frame, const GraphicStyle &style,
                    const WritingState &state) {
  const DrawingLine line = frame.line().value_or(DrawingLine());

  state.out().write_element_begin(
      "svg", HtmlElementOptions()
                 .set_attributes(HtmlAttributesVector{
                     {"xmlns", "http://www.w3.org/2000/svg"},
                     {"version", "1.1"},
                     {"overflow", "visible"}})
                 .set_style("z-index:-1;position:absolute;top:0;left:0;" +
                            translate_drawing_style(style) +
                            translate_drawing_transform(frame.transform())));

  state.out().write_element_begin(
      "line",
      HtmlElementOptions()
          .set_close_type(HtmlCloseType::trailing)
          .set_attributes(HtmlAttributesVector{{"x1", line.x1.to_string()},
                                               {"y1", line.y1.to_string()},
                                               {"x2", line.x2.to_string()},
                                               {"y2", line.y2.to_string()}}));

  state.out().write_element_end("svg");

  // A line's own text sits at its middle; most carry an empty paragraph and
  // want no box at all.
  if (std::ranges::any_of(frame.children(), [](const Element &child) {
        return has_content(child.children());
      })) {
    const std::string middle =
        "position:absolute;left:calc((" + line.x1.to_string() + " + " +
        line.x2.to_string() + ")/2);top:calc((" + line.y1.to_string() + " + " +
        line.y2.to_string() + ")/2);transform:translate(-50%,-100%);";
    state.out().write_element_begin("div",
                                    HtmlElementOptions().set_style(middle));
    translate_children(frame.children(), state);
    state.out().write_element_end("div");
  }
}

void translate_custom_shape(const Frame &frame, const GraphicStyle &style,
                            const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_shape_properties(frame) +
                                            translate_drawing_style(style)));
  translate_children(frame.children(), state);

  if (const std::optional<DrawingPath> path = frame.path(); path.has_value()) {
    const auto number = [](const double value) {
      return util::number::to_string_significant(value, 7);
    };
    state.out().write_new_line();
    state.out().write_element_begin(
        "svg",
        HtmlElementOptions()
            .set_attributes(HtmlAttributesVector{
                {"xmlns", "http://www.w3.org/2000/svg"},
                {"version", "1.1"},
                {"overflow", "visible"},
                {"preserveAspectRatio", "none"},
                {"viewBox", number(path->x) + " " + number(path->y) + " " +
                                number(path->width) + " " +
                                number(path->height)}})
            .set_style("z-index:-1;width:inherit;height:inherit;position:"
                       "absolute;top:0;left:0;padding:inherit;"));
    HtmlAttributesVector attributes{
        {"d", path->data},
        // A ring is two subpaths, and only even-odd leaves its hole.
        {"fill-rule", "evenodd"},
        // The view box scales, and not evenly; the stroke must not.
        {"vector-effect", "non-scaling-stroke"}};
    // An outline that never closes is a line, which svg would else fill as if
    // it did.
    if (path->data.find_first_of("Zz") == std::string::npos) {
      attributes.emplace_back("fill", "none");
    }
    state.out().write_element_begin("path",
                                    HtmlElementOptions()
                                        .set_close_type(HtmlCloseType::trailing)
                                        .set_attributes(std::move(attributes)));
    state.out().write_element_end("svg");
  }

  state.out().write_element_end("div");
}

} // namespace
} // namespace html

void html::translate_frame(const Element &element, const WritingState &state) {
  const Frame frame = element.as_frame();
  const GraphicStyle style = frame.style();

  switch (frame.shape_type()) {
  case ShapeType::none:
    translate_plain_frame(frame, style, state);
    break;
  case ShapeType::rect:
    translate_rect(frame, style, state);
    break;
  case ShapeType::ellipse:
    translate_ellipse(frame, style, state);
    break;
  case ShapeType::line:
    translate_line(frame, style, state);
    break;
  case ShapeType::custom:
    translate_custom_shape(frame, style, state);
    break;
  }
}

} // namespace odr::internal
