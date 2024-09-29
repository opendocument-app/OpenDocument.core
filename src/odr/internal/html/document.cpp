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
#include <utility>

namespace odr::internal::html {
namespace {

void front(const Document &document, HtmlWriter &out, const HtmlConfig &config,
           const HtmlResourceLocator &resourceLocator) {
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

  auto odr_css_file = Resources::open("odr.css");
  HtmlResourceLocation odr_css_location = resourceLocator(
      HtmlResourceType::css, "odr.css", "odr.css", odr_css_file, true);
  if (odr_css_location.has_value()) {
    out.write_header_style(odr_css_location.value());
  } else {
    out.write_header_style_begin();
    util::stream::pipe(*odr_css_file.stream(), out.out());
    out.write_header_style_end();
  }

  if (document.document_type() == DocumentType::spreadsheet) {
    auto odr_spreadsheet_css_file = Resources::open("odr_spreadsheet.css");
    HtmlResourceLocation odr_spreadsheet_css_location =
        resourceLocator(HtmlResourceType::css, "odr_spreadsheet.css",
                        "odr_spreadsheet.css", odr_spreadsheet_css_file, true);
    if (odr_spreadsheet_css_location.has_value()) {
      out.write_header_style(odr_spreadsheet_css_location.value());
    } else {
      util::stream::pipe(*odr_spreadsheet_css_file.stream(), out.out());
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

void back(const Document &document, html::HtmlWriter &out,
          const HtmlConfig &config,
          const HtmlResourceLocator &resourceLocator) {
  (void)document;
  (void)config;

  auto odr_js_file = Resources::open("odr.js");
  HtmlResourceLocation odr_js_location = resourceLocator(
      HtmlResourceType::js, "odr.js", "odr.js", odr_js_file, true);
  if (odr_js_location.has_value()) {
    out.write_script(odr_js_location.value());
  } else {
    out.write_script_begin();
    util::stream::pipe(*odr_js_file.stream(), out.out());
    out.write_script_end();
  }

  out.write_body_end();
  out.write_end();
}

std::string fill_path_variables(const std::string &path,
                                std::optional<std::uint32_t> index = {}) {
  std::string result = path;
  util::string::replace_all(result, "{index}",
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

class StaticHtmlService : public abstract::HtmlService {
public:
  StaticHtmlService(
      Document document,
      std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments)
      : m_document{std::move(document)}, m_fragments{std::move(fragments)} {}

  [[nodiscard]] const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
  fragments() const override {
    return m_fragments;
  }

  void write_html_document(
      HtmlWriter &out, const HtmlConfig &config,
      const HtmlResourceLocator &resourceLocator) const override {
    front(m_document, out, config, resourceLocator);
    for (const auto &fragment : m_fragments) {
      fragment->write_html_fragment(out, config, resourceLocator);
    }
    back(m_document, out, config, resourceLocator);
  }

private:
  Document m_document;
  const std::vector<std::shared_ptr<abstract::HtmlFragment>> m_fragments;
};

class HtmlFragmentBase : public abstract::HtmlFragment {
public:
  explicit HtmlFragmentBase(Document document)
      : m_document{std::move(document)} {}

  void
  write_html_document(HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    front(m_document, out, config, resourceLocator);
    write_html_fragment(out, config, resourceLocator);
    back(m_document, out, config, resourceLocator);
  }

protected:
  Document m_document;
};

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(Document document)
      : HtmlFragmentBase(std::move(document)) {}

  [[nodiscard]] std::string name() const final { return "document"; }

  void
  write_html_fragment(HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
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

      translate_children(element.children(), out, config, resourceLocator);

      out.write_element_end("div");
      out.write_element_end("div");
    } else {
      out.write_element_begin("div");
      translate_children(element.children(), out, config, resourceLocator);
      out.write_element_end("div");
    }
  }
};

class SlideHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SlideHtmlFragment(Document document, Slide slide)
      : HtmlFragmentBase(std::move(document)), m_slide{slide} {}

  [[nodiscard]] std::string name() const final { return m_slide.name(); }

  void
  write_html_fragment(HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    html::translate_slide(m_slide, out, config, resourceLocator);
  }

private:
  Slide m_slide;
};

class SheetHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SheetHtmlFragment(Document document, Sheet sheet)
      : HtmlFragmentBase(std::move(document)), m_sheet{sheet} {}

  [[nodiscard]] std::string name() const final { return m_sheet.name(); }

  void
  write_html_fragment(HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    translate_sheet(m_sheet, out, config, resourceLocator);
  }

private:
  Sheet m_sheet;
};

class PageHtmlFragment final : public HtmlFragmentBase {
public:
  explicit PageHtmlFragment(Document document, Page page)
      : HtmlFragmentBase(std::move(document)), m_page{page} {}

  [[nodiscard]] std::string name() const final { return m_page.name(); }

  void
  write_html_fragment(HtmlWriter &out, const HtmlConfig &config,
                      const HtmlResourceLocator &resourceLocator) const final {
    html::translate_page(m_page, out, config, resourceLocator);
  }

private:
  Page m_page;
};

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

  return HtmlService(std::make_unique<StaticHtmlService>(document, fragments));
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
    std::ofstream ostream(filled_path, std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    html::HtmlWriter out(ostream, config.format_html, config.html_indent);

    fragment.write_html_document(out.out(), config, resourceLocator);

    pages.emplace_back(fragment.name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
