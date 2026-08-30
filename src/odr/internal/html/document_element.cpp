#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>

namespace odr::internal {

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
  case ElementType::rect:
    translate_rect(element, state);
    break;
  case ElementType::line:
    translate_line(element, state);
    break;
  case ElementType::circle:
    translate_circle(element, state);
    break;
  case ElementType::custom_shape:
    translate_custom_shape(element, state);
    break;
  case ElementType::group:
    translate_children(element.children(), state);
    break;
  default:
    // TODO log
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
  state.out().write_element_begin("table",
                                  HtmlElementOptions().set_class("odr-sheet"));

  const TableDimensions rendered = sheet_rendered_extent(sheet, state.config());
  const std::uint32_t end_column = rendered.columns;
  const std::uint32_t end_row = rendered.rows;

  state.out().write_element_begin("col",
                                  HtmlElementOptions()
                                      .set_close_type(HtmlCloseType::none)
                                      .set_class("odr-sheet-gutter"));

  for (std::uint32_t column_index = 0; column_index < end_column;
       ++column_index) {
    const TableColumnStyle table_column_style =
        sheet.column_style(column_index);

    state.out().write_element_begin(
        "col",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::none)
            .set_style(translate_table_column_style(table_column_style)));
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

  TableCursor cursor;
  for (std::uint32_t row_index = cursor.row(); row_index < end_row;
       row_index = cursor.row()) {
    const TableRowStyle table_row_style = sheet.row_style(row_index);

    state.out().write_element_begin(
        "tr", HtmlElementOptions().set_style(
                  translate_table_row_style(table_row_style)));

    state.out().write_element_begin(
        "th", HtmlElementOptions()
                  .set_inline(true)
                  .set_class("odr-sheet-row-header")
                  .set_style([&]() -> std::optional<HtmlWritable> {
                    const std::optional<Measure> height =
                        table_row_style.height;
                    if (!height.has_value()) {
                      return std::nullopt;
                    }
                    return "height:" + height->to_string() +
                           ";max-height:" + height->to_string() + ";";
                  }()));
    state.out().write_raw(TablePosition::to_row_string(row_index));
    state.out().write_element_end("th");

    for (std::uint32_t column_index = cursor.column();
         column_index < end_column; column_index = cursor.column()) {
      const SheetCell cell = sheet.cell(column_index, row_index);

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
              .set_style(translate_table_cell_style(cell_style))
              .set_class([&]() -> std::optional<HtmlWritable> {
                if (cell_value_type == ValueType::float_number) {
                  return "odr-value-type-float";
                }
                return std::nullopt;
              }()));
      if (column_index == 0 && row_index == 0) {
        for (const Element shape : sheet.shapes()) {
          translate_element(shape, state);
        }
      }
      translate_children(cell.children(), state);
      state.out().write_element_end("td");

      cursor.add_cell(cell_span.columns, cell_span.rows);
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
      "x-s", HtmlElementOptions()
                 .set_inline(true)
                 .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                   if (state.config().editable && element.is_editable()) {
                     clb("contenteditable", "true");
                     clb("data-odr-path", element.document_path().to_string());
                   }
                 })
                 .set_style(translate_text_style(text.style())));
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

} // namespace

void html::translate_paragraph(const Element &element,
                               const WritingState &state,
                               const std::string &marker) {
  const Paragraph paragraph = element.as_paragraph();

  state.out().write_element_begin(
      "x-p",
      HtmlElementOptions().set_inline(true).set_style(
          "display:block;" + translate_paragraph_style(paragraph.style()) +
          translate_block_font_style(paragraph.text_style())));
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
  if (marker.empty() && !has_content(paragraph.children())) {
    // A line break, not a break opportunity: only a break is copied, so a blank
    // line between two paragraphs survives being pasted somewhere else.
    state.out().write_element_begin(
        "br", HtmlElementOptions().set_close_type(HtmlCloseType::none));
  } else {
    // A paragraph whose content is all out of flow has no line box of its own.
    state.out().write_element_begin(
        "wbr", HtmlElementOptions().set_close_type(HtmlCloseType::none));
  }
  state.out().write_element_end("x-p");
}

void html::translate_span(const Element &element, const WritingState &state) {
  const Span span = element.as_span();

  state.out().write_element_begin(
      "x-s", HtmlElementOptions().set_inline(true).set_style(
                 translate_text_style(span.style())));
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

void html::translate_frame(const Element &element, const WritingState &state) {
  const Frame frame = element.as_frame();
  const GraphicStyle style = frame.style();

  // A frame is a plain box, so its fill has to be a background - the `fill`
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

void html::translate_rect(const Element &element, const WritingState &state) {
  const Rect rect = element.as_rect();
  const GraphicStyle style = rect.style();

  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(translate_rect_properties(rect) +
                                            translate_drawing_style(style)));
  translate_children(rect.children(), state);
  state.out().write_new_line();
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)");
  state.out().write_element_end("div");
}

void html::translate_line(const Element &element, const WritingState &state) {
  const Line line = element.as_line();
  const GraphicStyle style = line.style();

  state.out().write_element_begin(
      "svg", HtmlElementOptions()
                 .set_attributes(HtmlAttributesVector{
                     {"xmlns", "http://www.w3.org/2000/svg"},
                     {"version", "1.1"},
                     {"overflow", "visible"}})
                 .set_style("z-index:-1;position:absolute;top:0;left:0;" +
                            translate_drawing_style(style) +
                            translate_drawing_transform(line.transform())));

  state.out().write_element_begin(
      "line",
      HtmlElementOptions()
          .set_close_type(HtmlCloseType::trailing)
          .set_attributes(HtmlAttributesVector{{"x1", line.x1().to_string()},
                                               {"y1", line.y1().to_string()},
                                               {"x2", line.x2().to_string()},
                                               {"y2", line.y2().to_string()}}));

  state.out().write_element_end("svg");

  // A line's own text sits at its middle; most carry an empty paragraph and
  // want no box at all.
  if (std::ranges::any_of(line.children(), [](const Element &child) {
        return has_content(child.children());
      })) {
    const std::string middle =
        "position:absolute;left:calc((" + line.x1().to_string() + " + " +
        line.x2().to_string() + ")/2);top:calc((" + line.y1().to_string() +
        " + " + line.y2().to_string() + ")/2);transform:translate(-50%,-100%);";
    state.out().write_element_begin("div",
                                    HtmlElementOptions().set_style(middle));
    translate_children(line.children(), state);
    state.out().write_element_end("div");
  }
}

void html::translate_circle(const Element &element, const WritingState &state) {
  const Circle circle = element.as_circle();
  const GraphicStyle style = circle.style();

  state.out().write_element_begin(
      "div",
      HtmlElementOptions().set_style(translate_circle_properties(circle) +
                                     translate_drawing_style(style)));
  state.out().write_new_line();
  translate_children(circle.children(), state);
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><ellipse cx="50%" cy="50%" rx="50%" ry="50%" /></svg>)");
  state.out().write_element_end("div");
}

void html::translate_custom_shape(const Element &element,
                                  const WritingState &state) {
  const CustomShape custom_shape = element.as_custom_shape();
  const GraphicStyle style = custom_shape.style();

  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(
                 translate_custom_shape_properties(custom_shape) +
                 translate_drawing_style(style)));
  translate_children(custom_shape.children(), state);

  if (const std::optional<DrawingPath> path = custom_shape.path();
      path.has_value()) {
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

} // namespace odr::internal
