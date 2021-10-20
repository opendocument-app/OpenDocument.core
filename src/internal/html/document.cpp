#include <fstream>
#include <internal/html/common.h>
#include <internal/html/document.h>
#include <internal/html/document_element.h>
#include <internal/html/document_style.h>
#include <internal/util/string_util.h>
#include <iostream>
#include <odr/document.h>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>

namespace odr::internal {

namespace {

void front(const Document &document, std::ostream &out,
           const HtmlConfig &config) {
  out << internal::html::doctype();
  out << "<html><head>";
  out << internal::html::default_headers();
  out << "<style>";
  out << internal::html::default_style();
  if (document.document_type() == DocumentType::spreadsheet) {
    out << internal::html::default_spreadsheet_style();
  }
  out << "</style>";
  out << "</head>";

  out << "<body " << internal::html::body_attributes(config) << ">";
}

void back(const Document &, std::ostream &out, const HtmlConfig &) {
  out << "<script>";
  out << internal::html::default_script();
  out << "</script>";

  out << "</body>";
  out << "</html>";
}

std::string fill_path_variables(const std::string &path,
                                std::optional<std::uint32_t> index = {}) {
  std::string result = path;
  internal::util::string::replace_all(result, "{index}",
                                      index ? std::to_string(*index) : "");
  return result;
}

std::ofstream output(const std::string &path,
                     std::optional<std::uint32_t> index = {}) {
  auto filled_path = fill_path_variables(path, index);
  std::ofstream out(filled_path);
  if (!out.is_open()) {
    throw FileWriteError();
  }
  return out;
}

} // namespace

Html html::translate_document(const Document &document, const std::string &path,
                              const HtmlConfig &config) {
  if (document.document_type() == DocumentType::text) {
    return internal::html::translate_text_document(document, path, config);
  } else if (document.document_type() == DocumentType::presentation) {
    return internal::html::translate_presentation(document, path, config);
  } else if (document.document_type() == DocumentType::spreadsheet) {
    return internal::html::translate_spreadsheet(document, path, config);
  } else if (document.document_type() == DocumentType::drawing) {
    return internal::html::translate_drawing(document, path, config);
  } else {
    throw UnknownDocumentType();
  }
}

Html html::translate_text_document(const Document &document,
                                   const std::string &path,
                                   const HtmlConfig &config) {
  auto filled_path =
      fill_path_variables(path + "/" + config.text_document_output_file_name);
  auto out = output(filled_path);

  auto cursor = document.root_element();
  auto element = cursor.element().text_root();

  front(document, out, config);
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
  back(document, out, config);

  return {document.file_type(), config, {{"document", filled_path}}, document};
}

Html html::translate_presentation(const Document &document,
                                  const std::string &path,
                                  const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto cursor = document.root_element();
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.slide_output_file_name, i);
    auto out = output(filled_path);

    front(document, out, config);
    internal::html::translate_slide(cursor, out, config);
    back(document, out, config);

    pages.emplace_back(cursor.element().slide().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_spreadsheet(const Document &document,
                                 const std::string &path,
                                 const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto cursor = document.root_element();
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.sheet_output_file_name, i);
    auto out = output(filled_path);

    front(document, out, config);
    translate_sheet(cursor, out, config);
    back(document, out, config);

    pages.emplace_back(cursor.element().sheet().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_drawing(const Document &document, const std::string &path,
                             const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto cursor = document.root_element();
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.page_output_file_name, i);
    auto out = output(filled_path);

    front(document, out, config);
    internal::html::translate_page(cursor, out, config);
    back(document, out, config);

    pages.emplace_back(cursor.element().page().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
