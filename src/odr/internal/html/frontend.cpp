#include <odr/internal/html/frontend.hpp>

#include <odr/error_code.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend_assets.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

namespace odr::internal::html {

namespace {

/// One of the renderer's own stylesheets or scripts — "shipped" in the sense
/// @ref odr::HtmlConfig::embed_shipped_resources means. @ref content is the
/// file of that name under `src/odr/internal/html/frontend/`, embedded by
/// `cmake/frontend_assets.cmake`.
struct Asset {
  HtmlResourceType type;
  std::string_view mime_type;
  std::string_view name;
  std::string_view content;
};

/// A newline ahead of the content, so an embedded asset and the file a service
/// ships are the same bytes.
std::string written(const Asset &asset) {
  return "\n" + std::string(asset.content);
}

/// Every page we write states its own body margin and background, rather than
/// reading whatever the browser's default sheet says.
constexpr Asset document_css_asset{HtmlResourceType::css, "text/css",
                                   "document.css",
                                   frontend_assets::document_css};
/// Dark counterpart of the style above. The `media` attribute of the element
/// carrying it gates it — a rule inside a query cannot beat an inline color.
constexpr Asset document_dark_css_asset{HtmlResourceType::css, "text/css",
                                        "document-dark.css",
                                        frontend_assets::document_dark_css};
/// No `text-overflow`: a `td`'s overflow is visible, so it would take clipping
/// to work.
constexpr Asset spreadsheet_css_asset{HtmlResourceType::css, "text/css",
                                      "spreadsheet.css",
                                      frontend_assets::spreadsheet_css};
constexpr Asset spreadsheet_dark_css_asset{
    HtmlResourceType::css, "text/css", "spreadsheet-dark.css",
    frontend_assets::spreadsheet_dark_css};
/// A whole-pixel line height, because the line numbers are a second column
/// whose cells are sized to the lines by script - a fractional line would round
/// per cell and the two columns would drift apart.
constexpr Asset text_css_asset{HtmlResourceType::css, "text/css", "text.css",
                               frontend_assets::text_css};
/// The gutter steps the other way in dark: lighter than the ground.
constexpr Asset text_dark_css_asset{HtmlResourceType::css, "text/css",
                                    "text-dark.css",
                                    frontend_assets::text_dark_css};
/// No numbered gutter - the numbers would be ours, not the file's. The column
/// carries the fold handles, and every line reserves it.
constexpr Asset xml_css_asset{HtmlResourceType::css, "text/css", "xml.css",
                              frontend_assets::xml_css};
constexpr Asset xml_dark_css_asset{HtmlResourceType::css, "text/css",
                                   "xml-dark.css",
                                   frontend_assets::xml_dark_css};
constexpr Asset filesystem_css_asset{HtmlResourceType::css, "text/css",
                                     "filesystem.css",
                                     frontend_assets::filesystem_css};
/// The washes lighten rather than darken: a black one paints nothing here.
constexpr Asset filesystem_dark_css_asset{HtmlResourceType::css, "text/css",
                                          "filesystem-dark.css",
                                          frontend_assets::filesystem_dark_css};
constexpr Asset media_css_asset{HtmlResourceType::css, "text/css", "media.css",
                                frontend_assets::media_css};
/// What the search script paints.
constexpr Asset search_css_asset{HtmlResourceType::css, "text/css",
                                 "search.css", frontend_assets::search_css};
/// The mark keeps its yellow; only the text on it turns over.
constexpr Asset search_dark_css_asset{HtmlResourceType::css, "text/css",
                                      "search-dark.css",
                                      frontend_assets::search_dark_css};
constexpr Asset document_js_asset{HtmlResourceType::js, "text/javascript",
                                  "document.js", frontend_assets::document_js};
/// The mode, the refusals and the log a save reads, generic over the formats.
constexpr Asset editing_js_asset{HtmlResourceType::js, "text/javascript",
                                 "editing.js", frontend_assets::editing_js};
/// Text search over the rendered page, format-agnostic: it walks text nodes.
constexpr Asset search_js_asset{HtmlResourceType::js, "text/javascript",
                                "search.js", frontend_assets::search_js};
/// Highlights the row and column under the pointer, and pins them on a click.
/// A column has no `:hover` selector, so one generated `:nth-child` rule lights
/// it — free per cell, but only correct while cell and column line up.
constexpr Asset spreadsheet_js_asset{HtmlResourceType::js, "text/javascript",
                                     "spreadsheet.js",
                                     frontend_assets::spreadsheet_js};
/// The cell overlay, and the locks the markup states because the page cannot
/// work them out. A sheet's editing is an overlay, not `contenteditable`.
constexpr Asset sheet_editing_js_asset{HtmlResourceType::js, "text/javascript",
                                       "sheet-editing.js",
                                       frontend_assets::sheet_editing_js};
/// Every input is applied to the line `<div>`s by hand, so the line numbers
/// stay in step and undo/redo replay changes instead of the browser's history.
constexpr Asset text_js_asset{HtmlResourceType::js, "text/javascript",
                              "text.js", frontend_assets::text_js};
/// The zoom api, and the fit where the css could not state it.
constexpr Asset viewport_js_asset{HtmlResourceType::js, "text/javascript",
                                  "viewport.js", frontend_assets::viewport_js};
/// The annotation overlays, and what `setOptions` writes into css.
constexpr Asset pdf_annotation_css_asset{HtmlResourceType::css, "text/css",
                                         "pdf-annotation.css",
                                         frontend_assets::pdf_annotation_css};
/// `odr.annotation`: the pending markup a viewer draws, and the payload
/// `PdfFile::annotate` takes. Geometry is kept in page-box points and mapped
/// to pdf user space only on the way out, through the `data-odr-space` each
/// page carries.
constexpr Asset pdf_annotation_js_asset{HtmlResourceType::js, "text/javascript",
                                        "pdf-annotation.js",
                                        frontend_assets::pdf_annotation_js};

/// Appends @p asset to @p resources; `nullopt` to embed it.
HtmlResourceLocation locate(const Asset &asset, const HtmlConfig &config,
                            HtmlResources &resources) {
  const odr::HtmlResource resource = HtmlResource::create(
      asset.type, std::string(asset.mime_type), std::string(asset.name),
      std::string(asset.name), odr::File::from_memory(written(asset)), true,
      false, true);
  HtmlResourceLocation location = config.resource_locator(resource, config);
  resources.emplace_back(resource, location);
  return location;
}

HtmlResources locate_all(const std::span<const Asset> assets,
                         const HtmlConfig &config) {
  HtmlResources resources;
  for (const Asset &asset : assets) {
    locate(asset, config, resources);
  }
  return resources;
}

/// Adds the dark sheets where the config asks for them.
HtmlResources locate_all(const std::span<const Asset> assets,
                         const std::span<const Asset> dark,
                         const HtmlConfig &config) {
  HtmlResources resources = locate_all(assets, config);
  if (writes_dark_style(config)) {
    for (const Asset &asset : dark) {
      locate(asset, config, resources);
    }
  }
  return resources;
}

/// @p media, when given, gates the stylesheet on that media query.
void write_style(const Asset &asset, const WritingState &state,
                 const std::string_view media = {}) {
  if (const HtmlResourceLocation location =
          locate(asset, state.config(), state.resources());
      location.has_value()) {
    state.out().write_header_style(xml::escape_attribute(*location), media);
    return;
  }

  state.out().write_header_style_begin(media);
  state.out().out() << written(asset);
  state.out().write_header_style_end();
}

void write_dark_style(const Asset &asset, const WritingState &state) {
  if (writes_dark_style(state.config())) {
    write_style(asset, state, dark_style_media(state.config()));
  }
}

/// The editing band of @ref odr::ErrorCode. Always inline, even where the
/// config links the scripts: it is per-render data, not an asset.
void write_error_codes(const WritingState &state) {
  state.out().write_script_begin();

  std::ostream &out = state.out().out();
  out << "\nwindow.odr = window.odr || {};\nwindow.odr.errorCodes = {";
  bool first = true;
  for (const ErrorCode code : all_error_codes()) {
    if (code < ErrorCode::edit_new_line) {
      continue;
    }
    out << (first ? "\n" : ",\n") << "  \"" << error_code_name(code)
        << "\": " << static_cast<std::int32_t>(code);
    first = false;
  }
  out << "\n};\n";

  state.out().write_script_end();
}

void write_script(const Asset &asset, const WritingState &state) {
  if (const HtmlResourceLocation location =
          locate(asset, state.config(), state.resources());
      location.has_value()) {
    state.out().write_script(xml::escape_attribute(*location));
    return;
  }

  state.out().write_script_begin();
  state.out().out() << written(asset);
  state.out().write_script_end();
}

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

bool html::writes_dark_style(const HtmlConfig &config) {
  return config.color_scheme != HtmlColorScheme::light;
}

std::string_view html::dark_style_media(const HtmlConfig &config) {
  return config.color_scheme == HtmlColorScheme::system
             ? "(prefers-color-scheme: dark)"
             : "";
}

void html::write_document_style(const WritingState &state) {
  write_style(document_css_asset, state);
}

void html::write_spreadsheet_style(const WritingState &state) {
  write_style(spreadsheet_css_asset, state);
}

void html::write_document_dark_style(const WritingState &state) {
  write_dark_style(document_dark_css_asset, state);
}

void html::write_spreadsheet_dark_style(const WritingState &state) {
  write_dark_style(spreadsheet_dark_css_asset, state);
}

void html::write_text_style(const WritingState &state) {
  write_style(text_css_asset, state);
}

void html::write_text_dark_style(const WritingState &state) {
  write_dark_style(text_dark_css_asset, state);
}

void html::write_xml_style(const WritingState &state) {
  write_style(xml_css_asset, state);
}

void html::write_xml_dark_style(const WritingState &state) {
  write_dark_style(xml_dark_css_asset, state);
}

void html::write_filesystem_style(const WritingState &state) {
  write_style(filesystem_css_asset, state);
}

void html::write_filesystem_dark_style(const WritingState &state) {
  write_dark_style(filesystem_dark_css_asset, state);
}

void html::write_media_style(const WritingState &state) {
  write_style(media_css_asset, state);
}

void html::write_search_style(const WritingState &state) {
  write_style(search_css_asset, state);
}

void html::write_search_dark_style(const WritingState &state) {
  write_dark_style(search_dark_css_asset, state);
}

void html::write_editing_script(const WritingState &state) {
  write_error_codes(state);
  write_script(editing_js_asset, state);
}

void html::write_document_script(const WritingState &state) {
  write_script(document_js_asset, state);
}

void html::write_search_script(const WritingState &state) {
  write_script(search_js_asset, state);
}

void html::write_spreadsheet_script(const WritingState &state) {
  write_script(spreadsheet_js_asset, state);
}

void html::write_sheet_editing_script(const WritingState &state) {
  write_script(sheet_editing_js_asset, state);
}

void html::write_text_script(const WritingState &state) {
  write_script(text_js_asset, state);
}

void html::write_pdf_annotation_style(const WritingState &state) {
  write_style(pdf_annotation_css_asset, state);
}

void html::write_pdf_annotation_script(const WritingState &state) {
  write_script(pdf_annotation_js_asset, state);
}

void html::write_viewport_script(const WritingState &state) {
  write_script(viewport_js_asset, state);
}

HtmlResources html::locate_text_resources(const HtmlConfig &config) {
  static constexpr std::array assets{text_css_asset,  search_css_asset,
                                     search_js_asset, editing_js_asset,
                                     text_js_asset,   viewport_js_asset};
  static constexpr std::array dark{text_dark_css_asset, search_dark_css_asset};
  return locate_all(assets, dark, config);
}

HtmlResources html::locate_xml_resources(const HtmlConfig &config) {
  static constexpr std::array assets{xml_css_asset, search_css_asset,
                                     search_js_asset, viewport_js_asset};
  static constexpr std::array dark{xml_dark_css_asset, search_dark_css_asset};
  return locate_all(assets, dark, config);
}

HtmlResources html::locate_search_resources(const HtmlConfig &config) {
  static constexpr std::array assets{search_css_asset, search_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_viewport_resources(const HtmlConfig &config) {
  static constexpr std::array assets{viewport_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_pdf_annotation_resources(const HtmlConfig &config) {
  static constexpr std::array assets{pdf_annotation_css_asset,
                                     pdf_annotation_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_media_resources(const HtmlConfig &config) {
  static constexpr std::array assets{media_css_asset};
  return locate_all(assets, config);
}

} // namespace odr::internal
