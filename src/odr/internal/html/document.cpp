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

void front(const Document &document, const WritingState &state) {
  state.out().write_begin();
  state.out().write_header_begin();
  state.out().write_header_charset("UTF-8");
  state.out().write_header_target("_blank");
  state.out().write_header_title("odr");
  if (document.document_type() == DocumentType::text &&
      state.config().text_document_margin) {
    state.out().write_header_viewport("width=device-width,user-scalable=yes");
  } else {
    state.out().write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
  }

  auto odr_css_file = Resources::open(common::Path("odr.css"));
  odr::HtmlResource odr_css_resource =
      html::HtmlResource::create(HtmlResourceType::css, "text/css", "odr.css",
                                 "odr.css", odr_css_file, true, true, false);
  HtmlResourceLocation odr_css_location =
      state.resource_locator()(odr_css_resource);
  state.resources().emplace_back(std::move(odr_css_resource), odr_css_location);
  if (odr_css_location.has_value()) {
    state.out().write_header_style(odr_css_location.value());
  } else {
    state.out().write_header_style_begin();
    util::stream::pipe(*odr_css_file.stream(), state.out().out());
    state.out().write_header_style_end();
  }

  if (document.document_type() == DocumentType::spreadsheet) {
    auto odr_spreadsheet_css_file =
        Resources::open(common::Path("odr_spreadsheet.css"));
    odr::HtmlResource odr_spreadsheet_css_resource = html::HtmlResource::create(
        HtmlResourceType::css, "text/css", "odr_spreadsheet.css",
        "odr_spreadsheet.css", odr_spreadsheet_css_file, true, true, false);
    HtmlResourceLocation odr_spreadsheet_css_location =
        state.resource_locator()(odr_spreadsheet_css_resource);
    state.resources().emplace_back(std::move(odr_spreadsheet_css_resource),
                                   odr_spreadsheet_css_location);
    if (odr_spreadsheet_css_location.has_value()) {
      state.out().write_header_style(odr_spreadsheet_css_location.value());
    } else {
      util::stream::pipe(*odr_spreadsheet_css_file.stream(), state.out().out());
    }
  }

  state.out().write_header_end();

  std::string body_clazz;
  switch (state.config().spreadsheet_gridlines) {
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

  state.out().write_body_begin(HtmlElementOptions().set_class(body_clazz));
}

void back(const Document &document, const WritingState &state) {
  (void)document;

  auto odr_js_file = Resources::open(common::Path("odr.js"));
  odr::HtmlResource odr_js_resource = html::HtmlResource::create(
      HtmlResourceType::js, "text/javascript", "odr.js", "odr.js", odr_js_file,
      true, true, false);
  HtmlResourceLocation odr_js_location =
      state.resource_locator()(odr_js_resource);
  state.resources().emplace_back(std::move(odr_js_resource), odr_js_location);
  if (odr_js_location.has_value()) {
    state.out().write_script(odr_js_location.value());
  } else {
    state.out().write_script_begin();
    util::stream::pipe(*odr_js_file.stream(), state.out().out());
    state.out().write_script_end();
  }

  state.out().write_body_end();
  state.out().write_end();
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

class HtmlFragmentBase : public HtmlFragmentService {
public:
  HtmlFragmentBase(std::string name, Document document, HtmlConfig config,
                   HtmlResourceLocator resource_locator)
      : HtmlFragmentService(std::move(config), std::move(resource_locator)),
        m_name{std::move(name)}, m_document{std::move(document)} {}

  const std::string &name() const { return m_name; }

  Document document() const { return m_document; }

protected:
  std::string m_name;
  Document m_document;
};

class HtmlDocumentServiceImpl final
    : public internal::abstract::HtmlDocumentService {
public:
  explicit HtmlDocumentServiceImpl(std::shared_ptr<HtmlFragmentBase> fragment)
      : m_fragment{std::move(fragment)} {}

  [[nodiscard]] const std::shared_ptr<HtmlFragmentBase> &fragment() const {
    return m_fragment;
  }

  [[nodiscard]] const HtmlConfig &config() const final {
    return m_fragment->config();
  }
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const final {
    return m_fragment->resource_locator();
  }

  HtmlResources write_document(HtmlWriter &out) const final {
    HtmlResources resources;

    WritingState state(out, config(), resource_locator(), resources);

    front(m_fragment->document(), state);
    m_fragment->write_fragment(out, resources);
    back(m_fragment->document(), state);

    return resources;
  }

private:
  std::shared_ptr<HtmlFragmentBase> m_fragment;
};

class HtmlFragmentCollection : public HtmlFragmentBase {
public:
  HtmlFragmentCollection(
      Document document,
      std::vector<std::shared_ptr<HtmlFragmentBase>> fragments,
      HtmlConfig config, HtmlResourceLocator resource_locator)
      : HtmlFragmentBase("document", std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_fragments{std::move(fragments)} {}

  [[nodiscard]] const std::vector<std::shared_ptr<HtmlFragmentBase>> &
  fragments() const {
    return m_fragments;
  }

  void write_fragment(HtmlWriter &out, HtmlResources &resources) const final {
    for (const auto &fragment : m_fragments) {
      fragment->write_fragment(out, resources);
    }
  }

private:
  std::vector<std::shared_ptr<HtmlFragmentBase>> m_fragments;
};

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(Document document, HtmlConfig config,
                            HtmlResourceLocator resource_locator)
      : HtmlFragmentBase("document", std::move(document), std::move(config),
                         std::move(resource_locator)) {}

  void write_fragment(HtmlWriter &out, HtmlResources &resources) const final {
    WritingState state(out, config(), resource_locator(), resources);

    auto root = m_document.root_element();
    auto element = root.text_root();

    if (config().text_document_margin) {
      auto page_layout = element.page_layout();
      page_layout.height = {};

      out.write_element_begin("div",
                              HtmlElementOptions().set_style(
                                  translate_outer_page_style(page_layout)));
      out.write_element_begin("div",
                              HtmlElementOptions().set_style(
                                  translate_inner_page_style(page_layout)));

      translate_children(element.children(), state);

      out.write_element_end("div");
      out.write_element_end("div");
    } else {
      out.write_element_begin("div");
      translate_children(element.children(), state);
      out.write_element_end("div");
    }
  }
};

class SlideHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SlideHtmlFragment(Document document, Slide slide, HtmlConfig config,
                             HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(slide.name(), std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_slide{slide} {}

  void write_fragment(HtmlWriter &out, HtmlResources &resources) const final {
    WritingState state(out, config(), resource_locator(), resources);

    html::translate_slide(m_slide, state);
  }

private:
  Slide m_slide;
};

class SheetHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SheetHtmlFragment(Document document, Sheet sheet, HtmlConfig config,
                             HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(sheet.name(), std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_sheet{sheet} {}

  void write_fragment(HtmlWriter &out, HtmlResources &resources) const final {
    WritingState state(out, config(), resource_locator(), resources);

    translate_sheet(m_sheet, state);
  }

private:
  Sheet m_sheet;
};

class PageHtmlFragment final : public HtmlFragmentBase {
public:
  explicit PageHtmlFragment(Document document, Page page, HtmlConfig config,
                            HtmlResourceLocator resource_locator)
      : HtmlFragmentBase(page.name(), std::move(document), std::move(config),
                         std::move(resource_locator)),
        m_page{page} {}

  void write_fragment(HtmlWriter &out, HtmlResources &resources) const final {
    WritingState state(out, config(), resource_locator(), resources);

    html::translate_page(m_page, state);
  }

private:
  Page m_page;
};

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

odr::HtmlDocumentService
html::create_document_service(const Document &document,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  HtmlResourceLocator resource_locator =
      local_resource_locator(output_path, config);

  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments;

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

  return odr::HtmlDocumentService(std::make_unique<HtmlDocumentServiceImpl>(
      std::make_unique<HtmlFragmentCollection>(document, fragments, config,
                                               resource_locator)));
}

Html html::translate_document(const odr::Document &document,
                              const std::string &output_path,
                              const odr::HtmlConfig &config) {
  odr::HtmlDocumentService document_service =
      create_document_service(document, output_path, config);
  auto document_service_impl =
      std::dynamic_pointer_cast<HtmlDocumentServiceImpl>(
          document_service.impl());
  auto fragment_service_impl =
      std::dynamic_pointer_cast<HtmlFragmentCollection>(
          document_service_impl->fragment());

  std::vector<HtmlPage> pages;

  std::uint32_t i = 0;
  for (const auto &fragment : fragment_service_impl->fragments()) {
    std::string filled_path = get_output_path(document, i, output_path, config);
    std::ofstream ostream(filled_path, std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    html::HtmlWriter out(ostream, config.format_html, config.html_indent);

    HtmlDocumentServiceImpl(fragment).write_document(out);

    pages.emplace_back(fragment->name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
