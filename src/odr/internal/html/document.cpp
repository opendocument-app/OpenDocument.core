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
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/resource.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <fstream>

namespace odr::internal::html {

namespace {

void front(const Document &document, HtmlWriter &out, const HtmlConfig &config,
           const HtmlResourceLocator &resource_locator,
           HtmlResources &resources) {
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

  auto odr_css_file = Resources::open(common::Path("odr.css"));
  HtmlResource odr_css_resource = common::HtmlResource::create(
      HtmlResourceType::css, "odr.css", "odr.css", odr_css_file, true, true);
  HtmlResourceLocation odr_css_location = resource_locator(odr_css_resource);
  resources.emplace_back(std::move(odr_css_resource), odr_css_location);
  if (odr_css_location.has_value()) {
    out.write_header_style(odr_css_location.value());
  } else {
    out.write_header_style_begin();
    util::stream::pipe(*odr_css_file.stream(), out.out());
    out.write_header_style_end();
  }

  if (document.document_type() == DocumentType::spreadsheet) {
    auto odr_spreadsheet_css_file =
        Resources::open(common::Path("odr_spreadsheet.css"));
    HtmlResource odr_spreadsheet_css_resource = common::HtmlResource::create(
        HtmlResourceType::css, "odr_spreadsheet.css", "odr_spreadsheet.css",
        odr_spreadsheet_css_file, true, true);
    HtmlResourceLocation odr_spreadsheet_css_location =
        resource_locator(odr_spreadsheet_css_resource);
    resources.emplace_back(std::move(odr_spreadsheet_css_resource),
                           odr_spreadsheet_css_location);
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
          const HtmlConfig &config, const HtmlResourceLocator &resource_locator,
          HtmlResources &resources) {
  (void)document;
  (void)config;

  auto odr_js_file = Resources::open(common::Path("odr.js"));
  HtmlResource odr_js_resource = common::HtmlResource::create(
      HtmlResourceType::js, "odr.js", "odr.js", odr_js_file, true, true);
  HtmlResourceLocation odr_js_location = resource_locator(odr_js_resource);
  resources.emplace_back(std::move(odr_js_resource), odr_js_location);
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
      std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments,
      HtmlConfig config, HtmlResourceLocator resource_locator)
      : m_document{std::move(document)}, m_fragments{std::move(fragments)},
        m_config{std::move(config)},
        m_resource_locator{std::move(resource_locator)} {}

  [[nodiscard]] const HtmlConfig &config() const final { return m_config; }
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const final {
    return m_resource_locator;
  }

  [[nodiscard]] const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
  fragments() const override {
    return m_fragments;
  }

  HtmlResources write_document(HtmlWriter &out) const override {
    HtmlResources resources;

    front(m_document, out, m_config, m_resource_locator, resources);
    for (const auto &fragment : m_fragments) {
      fragment->write_fragment(out);
    }
    back(m_document, out, m_config, m_resource_locator, resources);

    return resources;
  }

private:
  Document m_document;
  const std::vector<std::shared_ptr<abstract::HtmlFragment>> m_fragments;
  HtmlConfig m_config;
  HtmlResourceLocator m_resource_locator;
};

class HtmlFragmentBase : public abstract::HtmlFragment {
public:
  HtmlFragmentBase(Document document, HtmlConfig config,
                   HtmlResourceLocator resource_locator)
      : m_document{std::move(document)}, m_config{std::move(config)},
        m_resource_locator{std::move(resource_locator)} {}

  [[nodiscard]] const HtmlConfig &config() const final { return m_config; }
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const final {
    return m_resource_locator;
  }

  HtmlResources write_document(HtmlWriter &out) const final {
    HtmlResources resources;

    front(m_document, out, m_config, m_resource_locator, resources);
    write_fragment(out);
    back(m_document, out, m_config, m_resource_locator, resources);

    return resources;
  }

protected:
  Document m_document;
  HtmlConfig m_config;
  HtmlResourceLocator m_resource_locator;
};

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(Document document, HtmlConfig config,
                            HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(std::move(document), std::move(config),
                         std::move(resource_locator)) {}

  [[nodiscard]] std::string name() const final { return "document"; }

  HtmlResources write_fragment(HtmlWriter &out) const final {
    HtmlResources resources;

    auto root = m_document.root_element();
    auto element = root.text_root();

    if (m_config.text_document_margin) {
      auto page_layout = element.page_layout();
      page_layout.height = {};

      out.write_element_begin("div",
                              HtmlElementOptions().set_style(
                                  translate_outer_page_style(page_layout)));
      out.write_element_begin("div",
                              HtmlElementOptions().set_style(
                                  translate_inner_page_style(page_layout)));

      translate_children(element.children(), out, m_config, m_resource_locator,
                         resources);

      out.write_element_end("div");
      out.write_element_end("div");
    } else {
      out.write_element_begin("div");
      translate_children(element.children(), out, m_config, m_resource_locator,
                         resources);
      out.write_element_end("div");
    }

    return resources;
  }
};

class SlideHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SlideHtmlFragment(Document document, Slide slide, HtmlConfig config,
                             HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_slide{slide} {}

  [[nodiscard]] std::string name() const final { return m_slide.name(); }

  HtmlResources write_fragment(HtmlWriter &out) const final {
    HtmlResources resources;

    html::translate_slide(m_slide, out, m_config, m_resource_locator,
                          resources);

    return resources;
  }

private:
  Slide m_slide;
};

class SheetHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SheetHtmlFragment(Document document, Sheet sheet, HtmlConfig config,
                             HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_sheet{sheet} {}

  [[nodiscard]] std::string name() const final { return m_sheet.name(); }

  HtmlResources write_fragment(HtmlWriter &out) const final {
    HtmlResources resources;

    translate_sheet(m_sheet, out, m_config, m_resource_locator, resources);

    return resources;
  }

private:
  Sheet m_sheet;
};

class PageHtmlFragment final : public HtmlFragmentBase {
public:
  explicit PageHtmlFragment(Document document, Page page, HtmlConfig config,
                            HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_page{page} {}

  [[nodiscard]] std::string name() const final { return m_page.name(); }

  HtmlResources write_fragment(HtmlWriter &out) const final {
    HtmlResources resources;

    html::translate_page(m_page, out, m_config, m_resource_locator, resources);

    return resources;
  }

private:
  Page m_page;
};

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_document_service(const Document &document,
                                          const std::string &output_path,
                                          const HtmlConfig &config) {
  HtmlResourceLocator resource_locator =
      local_resource_locator(output_path, config);

  std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments;

  if (document.document_type() == DocumentType::text) {
    fragments.push_back(
        std::make_unique<TextHtmlFragment>(document, config, resource_locator));
  } else if (document.document_type() == DocumentType::presentation) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(std::make_unique<SlideHtmlFragment>(
          document, child.slide(), config, resource_locator));
    }
  } else if (document.document_type() == DocumentType::spreadsheet) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(std::make_unique<SheetHtmlFragment>(
          document, child.sheet(), config, resource_locator));
    }
  } else if (document.document_type() == DocumentType::drawing) {
    for (auto child : document.root_element().children()) {
      fragments.push_back(std::make_unique<PageHtmlFragment>(
          document, child.page(), config, resource_locator));
    }
  } else {
    throw UnknownDocumentType();
  }

  return HtmlService(std::make_unique<StaticHtmlService>(
      document, fragments, config, resource_locator));
}

Html html::translate_document(const odr::Document &document,
                              const std::string &output_path,
                              const odr::HtmlConfig &config) {
  HtmlService service = create_document_service(document, output_path, config);

  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (const auto &fragment : service.fragments()) {
    std::string filled_path = get_output_path(document, i, output_path, config);
    std::ofstream ostream(filled_path, std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    html::HtmlWriter out(ostream, config.format_html, config.html_indent);

    fragment.write_document(out.out());

    pages.emplace_back(fragment.name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
