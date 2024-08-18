#include <odr/internal/html/document.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/resource.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <fstream>

namespace odr::internal::html {
namespace {

class TextHtmlFragment final : public abstract::HtmlFragment {
public:
  explicit TextHtmlFragment(Document document)
      : m_document{std::move(document)} {}

  [[nodiscard]] std::string name() const final { return "document"; }

  void
  write_html_fragment(std::ostream &os, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    HtmlWriter out(os, config);

    auto root = m_document.root_element();
    auto element = root.text_root();

    if (config.text_document_margin) {
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
      out.write_element_begin("div");
      translate_children(element.children(), out, config);
      out.write_element_end("div");
    }
  }

private:
  Document m_document;
};

class SlideHtmlFragment final : public abstract::HtmlFragment {
public:
  explicit SlideHtmlFragment(Document document, Slide slide)
      : m_document{std::move(document)}, m_slide{slide} {}

  [[nodiscard]] std::string name() const final { return m_slide.name(); }

  void
  write_html_fragment(std::ostream &os, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    HtmlWriter out(os, config);

    internal::html::translate_slide(m_slide, out, config);
  }

private:
  Document m_document;
  Slide m_slide;
};

class SheetHtmlFragment final : public abstract::HtmlFragment {
public:
  explicit SheetHtmlFragment(Document document, Sheet sheet)
      : m_document{std::move(document)}, m_sheet{sheet} {}

  [[nodiscard]] std::string name() const final { return m_sheet.name(); }

  void
  write_html_fragment(std::ostream &os, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    HtmlWriter out(os, config);

    translate_sheet(m_sheet, out, config);
  }

private:
  Document m_document;
  Sheet m_sheet;
};

class PageHtmlFragment final : public abstract::HtmlFragment {
public:
  explicit PageHtmlFragment(Document document, Page page)
      : m_document{std::move(document)}, m_page{page} {}

  [[nodiscard]] std::string name() const final { return m_page.name(); }

  void
  write_html_fragment(std::ostream &os, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    HtmlWriter out(os, config);

    internal::html::translate_page(m_page, out, config);
  }

private:
  Document m_document;
  Page m_page;
};

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
      odr_css_path = odr_css_path.rebase(output_path);
    }
    out.write_header_style(odr_css_path.string());
    if (document.document_type() == DocumentType::spreadsheet) {
      auto odr_spreadsheet_css_path =
          common::Path(config.external_resource_path)
              .join("odr_spreadsheet.css");
      if (config.relative_resource_paths) {
        odr_spreadsheet_css_path =
            common::Path(odr_spreadsheet_css_path).rebase(output_path);
      }
      out.write_header_style(odr_spreadsheet_css_path.string());
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
      odr_js_path = odr_js_path.rebase(output_path);
    }
    out.write_script(odr_js_path.string());
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

std::string get_output_path(const Document &document, std::uint32_t index,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  if (document.document_type() == DocumentType::text) {
    return fill_path_variables(output_path + "/" +
                               config.text_document_output_file_name);
  } else if (document.document_type() == DocumentType::presentation) {
    return fill_path_variables(
        output_path + "/" + config.slide_output_file_name, index);
  } else if (document.document_type() == DocumentType::spreadsheet) {
    return fill_path_variables(
        output_path + "/" + config.sheet_output_file_name, index);
  } else if (document.document_type() == DocumentType::drawing) {
    return fill_path_variables(output_path + "/" + config.page_output_file_name,
                               index);
  } else {
    throw UnknownDocumentType();
  }
}

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::translate_document(const Document &document) {
  std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments;

  if (document.document_type() == DocumentType::text) {
    fragments.push_back(std::make_unique<TextHtmlFragment>(document));
  } else if (document.document_type() == DocumentType::presentation) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(
          std::make_unique<SlideHtmlFragment>(document, child.slide()));
    }
  } else if (document.document_type() == DocumentType::spreadsheet) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(
          std::make_unique<SheetHtmlFragment>(document, child.sheet()));
    }
  } else if (document.document_type() == DocumentType::drawing) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(
          std::make_unique<PageHtmlFragment>(document, child.page()));
    }
  } else {
    throw UnknownDocumentType();
  }

  return HtmlService(std::make_unique<StaticHtmlService>(fragments));
}

Html html::translate_document(const odr::Document &document,
                              const std::string &output_path,
                              const odr::HtmlConfig &config) {
  HtmlService service = translate_document(document);
  HtmlResourceLocator resourceLocator =
      local_resource_locator(output_path, config);

  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (const auto &fragment : service.fragments()) {
    std::string filled_path = get_output_path(document, i, output_path, config);
    std::ofstream ostream(filled_path);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    internal::html::HtmlWriter out(ostream, config.format_html,
                                   config.html_indent);

    front(document, output_path, out, config);
    fragment.write_html_fragment(out.out(), config, resourceLocator);
    back(document, output_path, out, config);

    pages.emplace_back(fragment.name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
