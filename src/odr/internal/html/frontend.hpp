#pragma once

namespace odr::internal::html {

struct WritingState;

/// Each of these writes one complete `<style>`/`<script>` element, or links it
/// as a shipped @ref odr::HtmlResource when the config asks for that.

void write_document_style(const WritingState &state);
/// Written in addition to the document style.
void write_spreadsheet_style(const WritingState &state);
void write_text_style(const WritingState &state);
void write_media_style(const WritingState &state);

/// The `odr` object a document view exposes to its host: `generateDiff()`,
/// `search()`, `searchNext()`, `searchPrevious()`, `resetSearch()`.
void write_document_script(const WritingState &state);
void write_text_script(const WritingState &state);

} // namespace odr::internal::html
