#include <internal/html/document.h>
#include <internal/html/document_element.h>
#include <internal/html/document_style.h>
#include <iostream>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/html.h>

namespace odr::internal {

void html::translate_text_document(DocumentCursor &cursor, std::ostream &out,
                                   const HtmlConfig &config) {
  auto element = cursor.element().text_root();

  if (config.text_document_margin) {
    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(element.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(element.page_layout()));
    out << ">";
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  } else {
    translate_children(cursor, out, config);
  }
}

void html::translate_presentation(DocumentCursor &cursor, std::ostream &out,
                                  const HtmlConfig &config) {
  // TODO indexing is kind of ugly here and duplicated
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }
    auto slide = cursor.element().slide();

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(slide.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(slide.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}

void html::translate_spreadsheet(DocumentCursor &cursor, std::ostream &out,
                                 const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }

    translate_sheet(cursor, out, config);
  });
}

void html::translate_drawing(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }
    auto page = cursor.element().page();

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(page.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(page.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}

} // namespace odr::internal
