#pragma once

#include <odr/html.hpp>

#include <string_view>

namespace odr::internal::html {

struct WritingState;

/// Each of these writes one complete `<style>`/`<script>` element, or links it
/// as a shipped @ref odr::HtmlResource when the config asks for that.

/// Each `write_*_dark_style` follows the style it restates and writes nothing
/// unless the config asks for a scheme other than
/// @ref odr::HtmlColorScheme::light. The pdf view has none: it paints its page
/// backgrounds itself.

void write_document_style(const WritingState &state);
void write_document_dark_style(const WritingState &state);
/// Written in addition to the document style.
void write_spreadsheet_style(const WritingState &state);
void write_spreadsheet_dark_style(const WritingState &state);
void write_text_style(const WritingState &state);
void write_text_dark_style(const WritingState &state);
void write_xml_style(const WritingState &state);
void write_xml_dark_style(const WritingState &state);
void write_filesystem_style(const WritingState &state);
void write_filesystem_dark_style(const WritingState &state);
/// The media view is black in every scheme, and has no dark style.
void write_media_style(const WritingState &state);
/// Written by every view that writes the search script.
void write_search_style(const WritingState &state);
void write_search_dark_style(const WritingState &state);

/// For a view writing its style inline rather than as a shipped asset: whether
/// to write a dark one at all, and the `media` gating it — empty where the
/// config asks for dark outright.
bool writes_dark_style(const HtmlConfig &config);
std::string_view dark_style_media(const HtmlConfig &config);

/// The `odr` object a document view exposes to its host: `generateDiff()`.
void write_document_script(const WritingState &state);
/// Written in addition to the document script.
void write_spreadsheet_script(const WritingState &state);
void write_text_script(const WritingState &state);
/// `odr.search()`, `searchNext()`, `searchPrevious()`, `resetSearch()` — the
/// rest of that object, for every view rendering text, whatever the format.
void write_search_script(const WritingState &state);

/// What the corresponding `write_*` calls would link, without writing anything:
/// a service has to answer for these paths as well as for its views. Every
/// entry is located `nullopt` when the config embeds them.
HtmlResources locate_text_resources(const HtmlConfig &config);
HtmlResources locate_xml_resources(const HtmlConfig &config);
HtmlResources locate_media_resources(const HtmlConfig &config);
HtmlResources locate_search_resources(const HtmlConfig &config);

} // namespace odr::internal::html
