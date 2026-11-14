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
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>

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

/// A group the editor cannot change inside one it can, a field, is kept from
/// the caret.
void translate_group(const Element &element, const html::WritingState &state) {
  const Element parent = element.parent();
  const bool locked = !html::writes_edit_markup(element, state) && parent &&
                      html::writes_edit_markup(parent, state) &&
                      html::has_content(element.children());
  if (locked) {
    state.out().write_element_begin(
        "x-s", html::HtmlElementOptions().set_inline(true).set_attributes(
                   html::HtmlAttributesVector{{"contenteditable", "false"}}));
  }
  html::translate_children(element.children(), state);
  if (locked) {
    state.out().write_element_end("x-s");
  }
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
    translate_paragraph(element, state, element.as_paragraph().marker(),
                        "odr-heading-marker");
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
    translate_group(element, state);
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
    case ElementType::group:
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

/// The image at @p href in the document as a resource, and where it is linked
/// from; no location where it is embedded instead.
std::pair<odr::HtmlResource, HtmlResourceLocation>
locate_internal_image(const std::string &href, const std::optional<File> &file,
                      const html::WritingState &state) {
  // the location is resolved against the document, so an engine naming the
  // image by its absolute path in the container has to lose the root
  const std::string path = Path(href).make_relative().string();
  odr::HtmlResource resource =
      file.has_value()
          ? html::linked_image_resource(*file, path, state.logger())
          : html::HtmlResource::create(HtmlResourceType::image, "image/jpg",
                                       path, path, file, false, false, true);
  HtmlResourceLocation location =
      state.config().resource_locator(resource, state.config());
  if (location.has_value()) {
    // a location taken by anything but this same image embeds instead
    const odr::HtmlResource *existing =
        html::resource_at(state.resources(), *location);
    const bool same_image = existing != nullptr && !existing->is_shipped() &&
                            existing->path() == resource.path();
    if (html::uri_kind(*location) == html::UriKind::refused ||
        (!same_image &&
         html::resource_location_taken(state.resources(), *location))) {
      location.reset();
    }
  }
  return {std::move(resource), std::move(location)};
}

/// The class that shows @p bitmap behind a page box. The first page of a view
/// to show an image writes its rule, and the others share it.
std::string page_bitmap_class(const BitmapFill &bitmap,
                              const html::WritingState &state) {
  const auto data_url = [&] {
    std::ostringstream source;
    html::translate_image_src(bitmap.file, source, state.config(),
                              state.logger());
    return std::move(source).str();
  };
  std::string url = bitmap.href.empty() ? data_url() : std::string();
  std::unordered_map<std::string, std::string> &classes =
      state.page_backgrounds();
  const auto [it, first] =
      classes.try_emplace(bitmap.href.empty() ? url : bitmap.href,
                          fmt::format("odr-page-bg-{}", classes.size()));
  if (!first && !state.repeat_page_backgrounds()) {
    return it->second;
  }
  if (!bitmap.href.empty()) {
    auto [resource, location] =
        locate_internal_image(bitmap.href, bitmap.file, state);
    url = location.has_value() ? *location : data_url();
    state.resources().emplace_back(std::move(resource), std::move(location));
  }
  // more specific than the page box's own `background`
  state.out().write_element_begin("style");
  state.out().write_raw(".odr-page-outer." + it->second +
                            "{background-image:" + html::css_url(url) + "}",
                        false);
  state.out().write_element_end("style");
  return it->second;
}

/// The css painting @p background behind a page box of @p layout's size, but
/// for a bitmap's image, which `page_bitmap_class` gives.
std::string translate_page_background(const GraphicStyle &background,
                                      const PageLayout &layout) {
  std::string result;
  if (background.fill_color.has_value() && background.fill_color->alpha != 0) {
    result += "background-color:" + html::color(*background.fill_color) + ";";
  }
  if (!background.fill_bitmap.has_value()) {
    return result +
           html::translate_fill_image(background, layout.width, layout.height);
  }
  switch (background.fill_bitmap->repeat) {
  case BitmapRepeat::repeat:
    break;
  case BitmapRepeat::stretch:
    result += "background-size:100% 100%;background-repeat:no-repeat;";
    break;
  case BitmapRepeat::no_repeat:
    result += "background-repeat:no-repeat;background-position:center;";
    break;
  }
  return result;
}

/// A slide or a drawing page: the master page's content under the page's own,
/// inside one outer page box. There is no inner (margin) box — unlike a text
/// document, both anchor their children absolutely at page coordinates.
template <typename PageLike>
void translate_page_like(const PageLike &page,
                         const html::WritingState &state) {
  const std::optional<bool> near = state.page_chunk_near();
  const PageLayout layout = page.page_layout();
  const GraphicStyle background = page.background();
  std::string classes = "odr-page-outer";
  if (background.fill_bitmap.has_value()) {
    classes += " " + page_bitmap_class(*background.fill_bitmap, state);
  } else if (background.fill_gradient.has_value() ||
             background.fill_hatch.has_value()) {
    // the dark sheet clears a gradient or hatch, but keeps a picture
    classes += " odr-fill";
  }
  if (near.has_value()) {
    classes += html::chunk_classes(state.config(), *near);
  }
  html::HtmlElementOptions options;
  options.set_class(classes).set_style(
      html::translate_outer_page_style(layout) +
      translate_page_background(background, layout));
  // where a text box lands; not `data-odr-id`, which a range would take for
  // something it can remove
  if (state.config().editable && state.editable_markup()) {
    options.set_attributes(html::HtmlAttributesVector{
        {"data-odr-page", std::to_string(page.identifier())}});
  }
  state.out().write_element_begin("div", options);

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
                               const std::string &marker,
                               const std::string &marker_class) {
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
                   .set_class(marker_class)
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
      translate_paragraph(child, state, marker, "odr-list-marker");
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

namespace html {
namespace {

/// An embedded image that repeats: an svg `image` where first written, a
/// `use` of it after.
void write_shared_image(const Image &image, const WritingState &state) {
  std::unordered_map<std::string, std::string> &ids = *state.shared_images();
  const auto [id, first] = ids.try_emplace(
      image.href(), fmt::format("odr-shared-image-{}", ids.size()));

  state.out().write_element_begin(
      "svg", HtmlElementOptions().set_style(
                 "position:absolute;left:0;top:0;width:100%;height:100%"));
  if (first) {
    state.out().write_element_begin(
        "image",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::trailing)
            .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
              clb("id", id->second);
              clb("width", "100%");
              clb("height", "100%");
              clb("preserveAspectRatio", "none");
              clb("href", [&](std::ostream &o) {
                // reached only for internal images, which have a file
                // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
                translate_image_src(image.file().value(), o, state.config(),
                                    state.logger());
              });
            }));
  } else {
    state.out().write_element_begin(
        "use",
        HtmlElementOptions()
            .set_close_type(HtmlCloseType::trailing)
            .set_attributes(HtmlAttributesVector{{"href", "#" + id->second}}));
  }
  state.out().write_element_end("svg");
}

} // namespace
} // namespace html

void html::translate_image(const Element &element, const WritingState &state) {
  const Image image = element.as_image();

  odr::HtmlResource resource;
  HtmlResourceLocation resource_location;
  if (image.is_internal()) {
    std::tie(resource, resource_location) =
        locate_internal_image(image.href(), image.file(), state);
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

  if (!resource_location.has_value() && state.shared_images() != nullptr) {
    write_shared_image(image, state);
    return;
  }

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

/// A gradient or hatch as a layer under the shape's svg, masked to
/// @p svg_shape where one is given.
void translate_fill_layer(const Frame &frame, const GraphicStyle &style,
                          const std::string_view view_box,
                          const std::string_view svg_shape,
                          const WritingState &state) {
  const std::string image =
      translate_fill_image(style, frame.width(), frame.height());
  if (image.empty()) {
    return;
  }
  std::string paint = "z-index:-1;width:inherit;height:inherit;position:"
                      "absolute;top:0;left:0;padding:inherit;background-"
                      "origin:content-box;background-clip:content-box;";
  if (style.fill_color.has_value() && style.fill_color->alpha != 0) {
    paint += "background-color:" + color(*style.fill_color) + ";";
  }
  paint += image;
  if (!svg_shape.empty()) {
    paint += translate_fill_mask(view_box, svg_shape);
  }
  state.out().write_new_line();
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_class("odr-fill").set_style(paint));
  state.out().write_element_end("div");
}

/// Whether a shape paints a fill layer under its svg.
bool has_fill_layer(const GraphicStyle &style) {
  return style.fill_gradient.has_value() || style.fill_hatch.has_value();
}

/// The class the dark sheet clears a shape's shadow by, none without one.
std::optional<HtmlWritable> shadow_class(const GraphicStyle &style) {
  if (!style.shadow.has_value()) {
    return std::nullopt;
  }
  return "odr-shadow";
}

/// Whether a shape casts its shadow from a box around its fill layer and svg.
bool has_shadow_box(const GraphicStyle &style) {
  return style.shadow.has_value() && has_fill_layer(style);
}

/// Opens the box that casts the shadow of a shape with a fill layer, so the
/// shape casts one shadow that the layer's mask does not clip. Otherwise,
/// returns the shadow the svg casts itself.
std::string begin_shape_shadow(const Frame &frame, const GraphicStyle &style,
                               const WritingState &state) {
  if (!has_shadow_box(style)) {
    return translate_shadow(style, false, frame.transform());
  }
  state.out().write_new_line();
  state.out().write_element_begin(
      "div", HtmlElementOptions().set_style(
                 "z-index:-1;width:inherit;height:inherit;position:absolute;"
                 "top:0;left:0;padding:inherit;" +
                 translate_shadow(style, false, frame.transform())));
  return {};
}

void end_shape_shadow(const GraphicStyle &style, const WritingState &state) {
  if (has_shadow_box(style)) {
    state.out().write_element_end("div");
  }
}

/// The drawing style of a shape; with a layer under it the svg only strokes.
std::string shape_style(const GraphicStyle &style) {
  std::string result = translate_drawing_style(style);
  if (has_fill_layer(style)) {
    result += "fill:none;";
  }
  return result;
}

void translate_plain_frame(const Frame &frame, const GraphicStyle &style,
                           const WritingState &state) {
  // A plain frame is a box, so its fill has to be a background - the `fill`
  // that `translate_drawing_style` writes only reaches the svg a shape carries.
  std::string background;
  const bool solid =
      style.fill_color.has_value() && style.fill_color->alpha != 0;
  if (solid) {
    background = "background-color:" + color(*style.fill_color) + ";";
  }
  const std::string image =
      translate_fill_image(style, frame.width(), frame.height());
  background += image;
  // a box that paints all of itself casts its shadow, else what it holds and
  // the lines of a hatch cast one
  background += translate_shadow(
      style, solid || style.fill_gradient.has_value(), frame.transform());
  HtmlElementOptions options;
  if (!image.empty()) {
    options.set_class(style.shadow.has_value() ? "odr-fill odr-shadow"
                                               : "odr-fill");
  } else {
    options.set_class(shadow_class(style));
  }
  state.out().write_element_begin(
      "div", options.set_attributes(frame_attributes(frame, state))
                 .set_style(translate_frame_properties(frame) +
                            translate_drawing_style(style) + background));
  translate_children(frame.children(), state);
  state.out().write_element_end("div");
}

void translate_simple_shape(const Frame &frame, const GraphicStyle &style,
                            const WritingState &state) {
  const bool ellipse = frame.shape_type() == ShapeType::ellipse;
  state.out().write_element_begin(
      "div",
      HtmlElementOptions()
          .set_class(shadow_class(style))
          .set_attributes(frame_attributes(frame, state))
          .set_style(translate_shape_properties(frame) + shape_style(style)));
  translate_children(frame.children(), state);
  const std::string svg_shadow = begin_shape_shadow(frame, style, state);
  translate_fill_layer(
      frame, style, ellipse ? "0 0 2 2" : "",
      ellipse ? R"(<ellipse cx="1" cy="1" rx="1" ry="1"/>)" : "", state);
  state.out().write_new_line();
  state.out().write_raw(
      R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;)" +
      svg_shadow + R"(">)" +
      (ellipse ? R"(<ellipse cx="50%" cy="50%" rx="50%" ry="50%" />)"
               : R"(<rect x="0" y="0" width="100%" height="100%" />)") +
      "</svg>");
  end_shape_shadow(style, state);
  state.out().write_element_end("div");
}

void translate_line(const Frame &frame, const GraphicStyle &style,
                    const WritingState &state) {
  const DrawingLine line = frame.line().value_or(DrawingLine());

  state.out().write_element_begin(
      "svg", HtmlElementOptions()
                 .set_class(shadow_class(style))
                 .set_attributes([&](const HtmlAttributeWriterCallback &clb) {
                   clb("xmlns", "http://www.w3.org/2000/svg");
                   clb("version", "1.1");
                   clb("overflow", "visible");
                   write_edit_address(frame, state, clb);
                 })
                 .set_style("z-index:-1;position:absolute;top:0;left:0;" +
                            translate_drawing_style(style) +
                            translate_shadow(style, false, frame.transform()) +
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

/// A Fontwork line and the style of its first run.
struct TextPathLine {
  std::string text;
  std::optional<TextStyle> style;
};

/// Paragraphs and explicit line breaks each start a line, including empty ones.
void text_path_lines(const Element &element, std::vector<TextPathLine> &lines) {
  for (const Element child : element.children()) {
    if (child.type() == ElementType::paragraph ||
        child.type() == ElementType::line_break) {
      lines.emplace_back();
    }
    if (child.type() == ElementType::text && !lines.empty()) {
      const Text text = child.as_text();
      TextPathLine &line = lines.back();
      line.text += text.content();
      if (!line.style.has_value()) {
        line.style = text.style();
      }
    } else {
      text_path_lines(child, lines);
    }
  }
}

/// The paint of a Fontwork shape's glyphs: its fill and its outline, as
/// LibreOffice paints them. A gradient or hatch gives its first color.
std::string text_path_paint(const GraphicStyle &style,
                            const std::optional<TextStyle> &text_style) {
  std::string result = "fill:";
  if (style.fill_gradient.has_value()) {
    result += color(style.fill_gradient->start_color);
  } else if (style.fill_hatch.has_value()) {
    result += color(style.fill_hatch->color);
  } else if (style.fill_color.has_value()) {
    result += style.fill_color->alpha == 0 ? "none" : color(*style.fill_color);
  } else if (text_style.has_value() && text_style->font_color.has_value()) {
    result += color(*text_style->font_color);
  } else {
    result += "#000000";
  }
  result += ";";
  if (style.stroke_color.has_value() && style.stroke_color->alpha != 0) {
    result +=
        "stroke:" + color(*style.stroke_color) + ";stroke-width:" +
        (style.stroke_width.has_value() && style.stroke_width->magnitude() != 0
             ? css_length(*style.stroke_width)
             : std::string("1px")) +
        ";";
  }
  return result;
}

/// A Fontwork shape: its text, one svg element per line, painted with the
/// shape's fill and outline. The path only guides the text, so it is not
/// drawn. Text on a curve is laid out straight.
void translate_text_path_shape(const Frame &frame, const GraphicStyle &style,
                               const TextPathMode mode,
                               const WritingState &state) {
  std::vector<TextPathLine> lines;
  text_path_lines(frame, lines);

  state.out().write_element_begin(
      "div", HtmlElementOptions()
                 .set_class(shadow_class(style))
                 .set_attributes(frame_attributes(frame, state))
                 .set_style(translate_shape_properties(frame)));

  const std::optional<double> width = css_pixels(frame.width());
  const std::optional<double> height = css_pixels(frame.height());
  if (!lines.empty() && width.has_value() && height.has_value() && *width > 0 &&
      *height > 0) {
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
                {"viewBox", "0 0 " + number(*width) + " " + number(*height)}})
            .set_style("position:absolute;top:0;left:0;width:100%;"
                       "height:100%;" +
                       translate_shadow(style, false, frame.transform())));
    const double line_height = *height / static_cast<double>(lines.size());
    for (std::size_t i = 0; i < lines.size(); ++i) {
      const TextPathLine &line = lines[i];
      if (line.text.empty()) {
        continue;
      }
      const double top = line_height * static_cast<double>(i);
      HtmlAttributesVector attributes;
      std::string text_style = line.style.has_value()
                                   ? translate_text_style(*line.style)
                                   : std::string();
      if (mode == TextPathMode::normal) {
        attributes.emplace_back("x", number(*width / 2));
        attributes.emplace_back("y", number(top + line_height / 2));
        attributes.emplace_back("text-anchor", "middle");
        attributes.emplace_back("dominant-baseline", "central");
      } else {
        attributes.emplace_back("x", "0");
        attributes.emplace_back("textLength", number(*width));
        attributes.emplace_back("lengthAdjust", "spacingAndGlyphs");
        if (mode == TextPathMode::shape) {
          // the line's ascent and descent fill its share of the height
          const double font_size = line_height / 1.15;
          attributes.emplace_back("y", number(top + font_size * 0.92));
          text_style += "font-size:" + number(font_size) + "px;";
        } else {
          attributes.emplace_back("y", number(top + line_height / 2));
          attributes.emplace_back("dominant-baseline", "central");
        }
      }
      attributes.emplace_back("vector-effect", "non-scaling-stroke");
      state.out().write_element_begin(
          "text", HtmlElementOptions()
                      .set_class("odr-text-path")
                      .set_attributes(std::move(attributes))
                      .set_style(text_style + "white-space:pre;" +
                                 text_path_paint(style, line.style)));
      state.out().write_raw(escape_text(line.text));
      state.out().write_element_end("text");
    }
    state.out().write_element_end("svg");
  }

  state.out().write_element_end("div");
}

void translate_custom_shape(const Frame &frame, const GraphicStyle &style,
                            const WritingState &state) {
  state.out().write_element_begin(
      "div",
      HtmlElementOptions()
          .set_class(shadow_class(style))
          .set_attributes(frame_attributes(frame, state))
          .set_style(translate_shape_properties(frame) + shape_style(style)));
  translate_children(frame.children(), state);

  if (const std::optional<DrawingPath> path = frame.path(); path.has_value()) {
    const auto number = [](const double value) {
      return util::number::to_string_significant(value, 7);
    };
    // An outline that never closes is a line, which svg would else fill as if
    // it did.
    const bool open = path->data.find_first_of("Zz") == std::string::npos;
    const std::string svg_shadow = begin_shape_shadow(frame, style, state);
    if (!open) {
      translate_fill_layer(frame, style,
                           number(path->x) + " " + number(path->y) + " " +
                               number(path->width) + " " + number(path->height),
                           R"(<path fill-rule="evenodd" d=")" +
                               xml::escape_attribute(path->data) + R"("/>)",
                           state);
    }
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
                       "absolute;top:0;left:0;padding:inherit;" +
                       svg_shadow));
    HtmlAttributesVector attributes{
        {"d", path->data},
        // A ring is two subpaths, and only even-odd leaves its hole.
        {"fill-rule", "evenodd"},
        // The view box scales, and not evenly; the stroke must not.
        {"vector-effect", "non-scaling-stroke"}};
    if (open) {
      attributes.emplace_back("fill", "none");
    }
    state.out().write_element_begin("path",
                                    HtmlElementOptions()
                                        .set_close_type(HtmlCloseType::trailing)
                                        .set_attributes(std::move(attributes)));
    state.out().write_element_end("svg");
    end_shape_shadow(style, state);
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
  case ShapeType::ellipse:
    translate_simple_shape(frame, style, state);
    break;
  case ShapeType::line:
    translate_line(frame, style, state);
    break;
  case ShapeType::custom:
    if (const std::optional<TextPathMode> text_path = frame.text_path();
        text_path.has_value()) {
      translate_text_path_shape(frame, style, *text_path, state);
    } else {
      translate_custom_shape(frame, style, state);
    }
    break;
  }
}

} // namespace odr::internal
