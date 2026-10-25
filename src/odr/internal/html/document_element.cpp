#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <optional>
#include <string>

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

bool html::has_content(const ElementRange &children) {
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

void html::write_paragraph_line_box(const bool empty,
                                    const WritingState &state) {
  state.out().write_element_begin(
      empty ? "br" : "wbr",
      HtmlElementOptions().set_close_type(HtmlCloseType::none));
}

bool html::writes_edit_markup(const Element &element,
                              const WritingState &state) {
  return state.editable_markup() && state.config().editable &&
         element.is_editable();
}

namespace {

/// The address an edit operation names @p element by, where the render offers
/// editing at all.
void write_edit_address(const Element &element, const html::WritingState &state,
                        const html::HtmlAttributeWriterCallback &clb) {
  if (html::writes_edit_markup(element, state)) {
    clb("data-odr-id", std::to_string(element.identifier()));
  }
}

/// The attributes a drawing's box takes: its address, so an operation names
/// the drawing rather than the runs inside it.
html::HtmlAttributeCallback frame_attributes(const Frame &frame,
                                             const html::WritingState &state) {
  return [&frame, &state](const html::HtmlAttributeWriterCallback &clb) {
    write_edit_address(frame, state, clb);
  };
}

} // namespace

namespace {

/// A slide or a drawing page: the master page's content under the page's own,
/// inside one outer page box. There is no inner (margin) box — unlike a text
/// document, both anchor their children absolutely at page coordinates.
template <typename PageLike>
void translate_page_like(const PageLike &page,
                         const html::WritingState &state) {
  const std::optional<bool> near = state.page_chunk_near();
  state.out().write_element_begin(
      "div",
      html::HtmlElementOptions()
          .set_class("odr-page-outer" +
                     (near.has_value()
                          ? html::chunk_classes(state.config(), *near)
                          : std::string()))
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
            write_edit_address(element, state, clb);
          })
          .set_style(translate_text_style(text.style()), state.styles()));
  // the editor reads the run's text back, so it is written as is
  state.out().out() << xml::escape_text(text.content());
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
      HtmlElementOptions()
          .set_inline(true)
          .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
            write_edit_address(element, state, clb);
          })
          .set_style("display:block;" +
                         translate_paragraph_style(paragraph.style(),
                                                   state.direction()) +
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
  const LinkTarget target = link_target(link.href(), state.config());

  // A refused target loses the attribute, not the element.
  HtmlAttributesVector attributes;
  if (target.href.has_value()) {
    attributes.emplace_back("href", xml::escape_attribute(*target.href));
  }

  HtmlElementOptions options =
      HtmlElementOptions().set_inline(true).set_attributes(
          std::move(attributes));
  if (!target.attributes.empty()) {
    options.set_extra(std::string(target.attributes));
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
              .set_style(translate_table_cell_style(
                  table_cell.style(), writes_dark_style(state.config()))));

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
    const std::optional<File> file = image.file();
    resource = file.has_value()
                   ? linked_image_resource(*file, path, state.logger())
                   : HtmlResource::create(HtmlResourceType::image, "image/jpg",
                                          path, path, file, false, false, true);
    resource_location =
        state.config().resource_locator(resource, state.config());
    if (resource_location.has_value()) {
      // a location taken by anything but this same image embeds instead
      const odr::HtmlResource *existing =
          resource_at(state.resources(), *resource_location);
      const bool same_image = existing != nullptr && !existing->is_shipped() &&
                              existing->path() == resource.path();
      if (uri_kind(*resource_location) == UriKind::refused ||
          (!same_image &&
           resource_location_taken(state.resources(), *resource_location))) {
        resource_location.reset();
      }
    }
  } else if (loads_external_source(image.href(), state.config())) {
    resource =
        HtmlResource::create(HtmlResourceType::image, "image/jpg", "image",
                             "image", std::nullopt, false, false, false);
    resource_location = image.href();
  } else {
    state.out().write_element_begin(
        "img", HtmlElementOptions()
                   .set_close_type(HtmlCloseType::trailing)
                   .set_attributes(HtmlAttributesVector{
                       {"alt", "external image not loaded"}})
                   .set_style("position:absolute;left:0;top:0;width:100%;"
                              "height:100%"));
    return;
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
              // fetched only near the viewport (`lazy-loading.md` decision 5)
              clb("loading", "lazy");
              clb("decoding", "async");
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
      "div", HtmlElementOptions()
                 .set_attributes(frame_attributes(frame, state))
                 .set_style(translate_frame_properties(frame) +
                            translate_drawing_style(style) + background));
  translate_children(frame.children(), state);
  state.out().write_element_end("div");
}

void translate_rect(const Frame &frame, const GraphicStyle &style,
                    const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_attributes(frame_attributes(frame, state))
                 .set_style(translate_shape_properties(frame) +
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
      "div", HtmlElementOptions()
                 .set_attributes(frame_attributes(frame, state))
                 .set_style(translate_shape_properties(frame) +
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
                 .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                   clb("xmlns", "http://www.w3.org/2000/svg");
                   clb("version", "1.1");
                   clb("overflow", "visible");
                   write_edit_address(frame, state, clb);
                 })
                 .set_style("z-index:-1;position:absolute;top:0;left:0;" +
                            translate_drawing_style(style) +
                            translate_drawing_transform(frame.transform())));

  state.out().write_element_begin(
      "line",
      HtmlElementOptions()
          .set_close_type(HtmlCloseType::trailing)
          .set_attributes(HtmlAttributesVector{{"x1", css_length(line.x1)},
                                               {"y1", css_length(line.y1)},
                                               {"x2", css_length(line.x2)},
                                               {"y2", css_length(line.y2)}}));

  state.out().write_element_end("svg");

  // A line's own text sits at its middle; most carry an empty paragraph and
  // want no box at all.
  if (std::ranges::any_of(frame.children(), [](const Element &child) {
        return has_content(child.children());
      })) {
    const std::string middle =
        "position:absolute;left:calc((" + css_length(line.x1) + " + " +
        css_length(line.x2) + ")/2);top:calc((" + css_length(line.y1) + " + " +
        css_length(line.y2) + ")/2);transform:translate(-50%,-100%);";
    state.out().write_element_begin("div",
                                    HtmlElementOptions().set_style(middle));
    translate_children(frame.children(), state);
    state.out().write_element_end("div");
  }
}

void translate_custom_shape(const Frame &frame, const GraphicStyle &style,
                            const WritingState &state) {
  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_attributes(frame_attributes(frame, state))
                 .set_style(translate_shape_properties(frame) +
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
