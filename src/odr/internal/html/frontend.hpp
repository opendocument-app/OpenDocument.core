#pragma once

#include <odr/html.hpp>

#include <string_view>

namespace odr::internal::html {

struct WritingState;

/// Each of these writes one complete `<style>`/`<script>` element, or links it
/// as a shipped @ref odr::HtmlResource when the config asks for that.

/// Each `write_*_dark_style` follows the style it restates and writes nothing
/// for @ref odr::HtmlColorScheme::light.

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
/// Black in every scheme, so no dark style.
void write_media_style(const WritingState &state);
/// Written by every view that writes the search script.
void write_search_style(const WritingState &state);
void write_search_dark_style(const WritingState &state);

/// For a view writing its style inline: whether to write a dark one, and the
/// `media` gating it — empty for @ref odr::HtmlColorScheme::dark.
bool writes_dark_style(const HtmlConfig &config);
std::string_view dark_style_media(const HtmlConfig &config);

/// `odr.editing`, `odr.onError`, `odr.takesKeys` and `odr.generateDiff`.
/// Written by every document view, and **first**: the scripts below read
/// `odr.takesKeys`.
void write_editing_script(const WritingState &state);
/// The text editor, attached to the mode. Written where the config edits.
void write_document_script(const WritingState &state);
/// Written in addition to the editing script.
void write_spreadsheet_script(const WritingState &state);
/// The sheet's editor, attached to the mode. Written where the config edits.
void write_sheet_editing_script(const WritingState &state);
void write_text_script(const WritingState &state);
/// `odr.search()`, `searchNext()`, `searchPrevious()`, `resetSearch()` — the
/// rest of that object, for every view rendering text, whatever the format.
void write_search_script(const WritingState &state);

/// `odr.annotation`: the pending markup a viewer draws on a pdf page, and
/// `getAnnotations()`, the payload @ref odr::PdfFile::annotate takes.
void write_pdf_annotation_style(const WritingState &state);
void write_pdf_annotation_script(const WritingState &state);

/// `odr.getZoom()`, `setZoom(value, focus)`, `adjustZoom(factor, focus)`,
/// `resetZoom(focus)`, `isZoomFitted()`, `getViewportRect(element)`,
/// `onZoomChange`, plus the fit @ref write_zoom_style left to be measured.
/// Holds the reading position.
void write_viewport_script(const WritingState &state);

/// What the corresponding `write_*` calls would link, without writing anything:
/// a service has to answer for these paths as well as for its views. Every
/// entry is located `nullopt` when the config embeds them.
HtmlResources locate_text_resources(const HtmlConfig &config);
HtmlResources locate_xml_resources(const HtmlConfig &config);
HtmlResources locate_media_resources(const HtmlConfig &config);
HtmlResources locate_search_resources(const HtmlConfig &config);
HtmlResources locate_viewport_resources(const HtmlConfig &config);
HtmlResources locate_pdf_annotation_resources(const HtmlConfig &config);

} // namespace odr::internal::html
