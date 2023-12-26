#include <odr/internal/html/document.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/resource.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <fstream>

namespace {

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::html;

void front(const Document &document, const std::string &output_path,
           HtmlWriter &out, const HtmlConfig &config) {
  out.write_begin();
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  if (document.document_type() == DocumentType::text &&
      config.text_document_margin) {
    out.write_header_viewport("width=device-width,user-scalable=yes");
  } else {
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
  }

  if (config.embed_resources) {
    out.write_header_style_begin();
    util::stream::pipe(
        *Resources::instance().filesystem()->open("odr.css")->stream(),
        out.out());
    if (document.document_type() == DocumentType::spreadsheet) {
      util::stream::pipe(*Resources::instance()
                              .filesystem()
                              ->open("odr_spreadsheet.css")
                              ->stream(),
                         out.out());
    }
    out.write_header_style_end();
  } else {
    auto odr_css_path =
        common::Path(config.external_resource_path).join("odr.css");
    if (config.relative_resource_paths) {
      odr_css_path = common::Path(odr_css_path).rebase(output_path);
    }
    out.write_header_style(odr_css_path.string().c_str());
    if (document.document_type() == DocumentType::spreadsheet) {
      auto odr_spreadsheet_css_path =
          common::Path(config.external_resource_path)
              .join("odr_spreadsheet.css");
      if (config.relative_resource_paths) {
        odr_spreadsheet_css_path =
            common::Path(odr_spreadsheet_css_path).rebase(output_path);
      }
      out.write_header_style(odr_spreadsheet_css_path.string().c_str());
    }
  }

  out.write_header_end();

  std::string body_clazz;
  switch (config.spreadsheet_gridlines) {
  case HtmlTableGridlines::soft:
    body_clazz = "odr-gridlines-soft";
    break;
  case HtmlTableGridlines::hard:
    body_clazz = "odr-gridlines-hard";
    break;
  case HtmlTableGridlines::none:
  default:
    body_clazz = "odr-gridlines-none";
    break;
  }

  out.write_body_begin(HtmlElementOptions().set_class(body_clazz));
}

void back(const Document &, const std::string &output_path,
          internal::html::HtmlWriter &out, const HtmlConfig &config) {
  if (config.embed_resources) {
    out.write_script_begin();
    util::stream::pipe(
        *Resources::instance().filesystem()->open("odr.js")->stream(),
        out.out());
    out.write_script_end();
  } else {
    auto odr_js_path =
        common::Path(config.external_resource_path).join("odr.js");
    if (config.relative_resource_paths) {
      odr_js_path = common::Path(odr_js_path).rebase(output_path);
    }
    out.write_script(odr_js_path.string().c_str());
  }

  out.write_body_end();
  out.write_end();
}

std::string fill_path_variables(const std::string &path,
                                std::optional<std::uint32_t> index = {}) {
  std::string result = path;
  internal::util::string::replace_all(result, "{index}",
                                      index ? std::to_string(*index) : "");
  return result;
}

std::ofstream output(const std::string &path) {
  std::ofstream out(path);
  if (!out.is_open()) {
    throw FileWriteError();
  }
  return out;
}

} // namespace

namespace odr::internal {

Html html::translate_document(const Document &document,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  if (document.document_type() == DocumentType::text) {
    return internal::html::translate_text_document(document, output_path,
                                                   config);
  } else if (document.document_type() == DocumentType::presentation) {
    return internal::html::translate_presentation(document, output_path,
                                                  config);
  } else if (document.document_type() == DocumentType::spreadsheet) {
    return internal::html::translate_spreadsheet(document, output_path, config);
  } else if (document.document_type() == DocumentType::drawing) {
    return internal::html::translate_drawing(document, output_path, config);
  } else {
    throw UnknownDocumentType();
  }
}

Html html::translate_text_document(const Document &document,
                                   const std::string &output_path,
                                   const HtmlConfig &config) {
  auto filled_path = fill_path_variables(output_path + "/" +
                                         config.text_document_output_file_name);
  auto ostream = output(filled_path);
  internal::html::HtmlWriter out(ostream, config.format_html,
                                 config.html_indent);

  auto root = document.root_element();
  auto element = root.text_root();

  front(document, output_path, out, config);
  if (config.text_document_margin) {
    out.write_element_begin("div");
    auto page_layout = element.page_layout();
    page_layout.height = {};

    out.write_element_begin("div",
                            HtmlElementOptions().set_style(
                                translate_outer_page_style(page_layout)));
    out.write_element_begin("div",
                            HtmlElementOptions().set_style(
                                translate_inner_page_style(page_layout)));

    translate_children(element.children(), out, config);

    out.write_element_end("div");
    out.write_element_end("div");
  } else {
    translate_children(element.children(), out, config);
  }
  back(document, output_path, out, config);

  return {document.file_type(), config, {{"document", filled_path}}, document};
}

Html html::translate_presentation(const Document &document,
                                  const std::string &output_path,
                                  const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (auto child : document.root_element().children()) {
    auto filled_path = fill_path_variables(
        output_path + "/" + config.slide_output_file_name, i);
    auto ostream = output(filled_path);
    internal::html::HtmlWriter out(ostream, config.format_html,
                                   config.html_indent);

    front(document, output_path, out, config);
    internal::html::translate_slide(child, out, config);
    back(document, output_path, out, config);

    pages.emplace_back(child.slide().name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_spreadsheet(const Document &document,
                                 const std::string &output_path,
                                 const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (auto child : document.root_element().children()) {
    auto filled_path = fill_path_variables(
        output_path + "/" + config.sheet_output_file_name, i);
    auto ostream = output(filled_path);
    internal::html::HtmlWriter out(ostream, config.format_html,
                                   config.html_indent);

    front(document, output_path, out, config);
    translate_sheet(child, out, config);
    back(document, output_path, out, config);

    pages.emplace_back(child.sheet().name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

Html html::translate_drawing(const Document &document,
                             const std::string &output_path,
                             const HtmlConfig &config) {
  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (auto child : document.root_element().children()) {
    auto filled_path = fill_path_variables(
        output_path + "/" + config.page_output_file_name, i);
    auto ostream = output(filled_path);
    internal::html::HtmlWriter out(ostream, config.format_html,
                                   config.html_indent);

    front(document, output_path, out, config);
    internal::html::translate_page(child, out, config);
    back(document, output_path, out, config);

    pages.emplace_back(child.page().name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
