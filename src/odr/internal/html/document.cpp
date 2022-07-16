#include <fstream>
#include <iostream>
#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/resource.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

namespace odr::internal {

namespace {

void front(const Document &document, const std::string &path, std::ostream &out,
           const HtmlConfig &config) {
  out << internal::html::doctype();
  out << "<html>\n";
  out << "<head>\n";
  if (document.document_type() == DocumentType::text &&
      config.text_document_margin) {
    out << R"V0G0N(<meta charset="UTF-8"/>
<base target="_blank"/>
<meta name="viewport" content="width=device-width,user-scalable=yes"/>
<title>odr</title>)V0G0N";
  } else {
    out << internal::html::default_headers();
  }
  out << "\n";

  if (config.embed_resources) {
    out << "<style>\n";
    util::stream::pipe(
        *Resources::instance().filesystem()->open("odr.css")->stream(), out);
    if (document.document_type() == DocumentType::spreadsheet) {
      util::stream::pipe(*Resources::instance()
                              .filesystem()
                              ->open("odr_spreadsheet.css")
                              ->stream(),
                         out);
    }
    out << "\n";
    out << "</style>\n";
  } else {
    auto odr_css_path =
        common::Path(config.external_resource_path).join("odr.css");
    if (config.relative_resource_paths) {
      odr_css_path = common::Path(odr_css_path).rebase(path);
    }
    out << "<link rel=\"stylesheet\" href=\"" << odr_css_path << "\">\n";
    if (document.document_type() == DocumentType::spreadsheet) {
      auto odr_spreadsheet_css_path =
          common::Path(config.external_resource_path)
              .join("odr_spreadsheet.css");
      if (config.relative_resource_paths) {
        odr_spreadsheet_css_path =
            common::Path(odr_spreadsheet_css_path).rebase(path);
      }
      out << "<link rel=\"stylesheet\" href=\"" << odr_spreadsheet_css_path
          << "\">\n";
    }
  }

  out << "</head>\n";
  out << "<body " << internal::html::body_attributes(config) << ">\n";
}

void back(const Document &, const std::string &path, std::ostream &out,
          const HtmlConfig &config) {
  out << "\n";

  if (config.embed_resources) {
    out << "<script type=\"text/javascript\">\n";
    util::stream::pipe(
        *Resources::instance().filesystem()->open("odr.js")->stream(), out);
    out << "\n";
    out << "</script>\n";
  } else {
    auto odr_js_path =
        common::Path(config.external_resource_path).join("odr.js");
    if (config.relative_resource_paths) {
      odr_js_path = common::Path(odr_js_path).rebase(path);
    }
    out << "<script type=\"text/javascript\" src=\"" << odr_js_path
        << "\"></script>";
  }

  out << "</body>\n";
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

  auto root = document.root_element();
  auto element = root.text_root();

  front(document, path, out, config);
  if (config.text_document_margin) {
    out << "<div";
    auto page_layout = element.page_layout();
    page_layout.height = {};
    out << optional_style_attribute(translate_outer_page_style(page_layout));
    out << ">";
    out << "<div";
    out << optional_style_attribute(translate_inner_page_style(page_layout));
    out << ">";
    translate_children(element, out, config);
    out << "</div>";
    out << "</div>";
  } else {
    translate_children(element, out, config);
  }
  back(document, path, out, config);

  return {document.file_type(), config, {{"document", filled_path}}, document};
}

Html html::translate_presentation(const Document &document,
                                  const std::string &path,
                                  const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto root = document.root_element();
  root.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.slide_output_file_name, i);
    auto out = output(filled_path);

    front(document, path, out, config);
    internal::html::translate_slide(cursor, out, config);
    back(document, path, out, config);

    pages.emplace_back(cursor.element().slide().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_spreadsheet(const Document &document,
                                 const std::string &path,
                                 const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto root = document.root_element();
  root.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.sheet_output_file_name, i);
    auto out = output(filled_path);

    front(document, path, out, config);
    translate_sheet(cursor, out, config);
    back(document, path, out, config);

    pages.emplace_back(cursor.element().sheet().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_drawing(const Document &document, const std::string &path,
                             const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  auto root = document.root_element();
  root.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    auto filled_path =
        fill_path_variables(path + "/" + config.page_output_file_name, i);
    auto out = output(filled_path);

    front(document, path, out, config);
    internal::html::translate_page(cursor, out, config);
    back(document, path, out, config);

    pages.emplace_back(cursor.element().page().name(), filled_path);
  });

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
