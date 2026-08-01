#pragma once

namespace odr::internal::html {

class HtmlWriter;

/// Each of these writes one complete `<style>`/`<script>` element.

void write_document_style(HtmlWriter &out);
/// Written in addition to the document style.
void write_spreadsheet_style(HtmlWriter &out);
void write_text_style(HtmlWriter &out);
void write_media_style(HtmlWriter &out);

/// The `odr` object a document view exposes to its host: `generateDiff()`,
/// `search()`, `searchNext()`, `searchPrevious()`, `resetSearch()`.
void write_document_script(HtmlWriter &out);
void write_text_script(HtmlWriter &out);

} // namespace odr::internal::html
