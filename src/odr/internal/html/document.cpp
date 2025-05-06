#include <odr/internal/html/document.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <fstream>

namespace odr::internal::html {

namespace {

void front(const Document &document, const WritingState &state) {
  bool paged_content = ((document.document_type() == DocumentType::text) &&
                        state.config().text_document_margin) ||
                       document.document_type() == DocumentType::presentation ||
                       document.document_type() == DocumentType::drawing;

  state.out().write_begin();
  state.out().write_header_begin();
  state.out().write_header_charset("UTF-8");
  state.out().write_header_target("_blank");
  state.out().write_header_title("odr");
  if (paged_content) {
    state.out().write_header_viewport("width=device-width,user-scalable=yes");
  } else {
    state.out().write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
  }

  auto odr_css_file = File(
      common::Path(state.config().resource_path).join(common::Path("odr.css")));
  odr::HtmlResource odr_css_resource =
      html::HtmlResource::create(HtmlResourceType::css, "text/css", "odr.css",
                                 "odr.css", odr_css_file, true, false, true);
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
        File(common::Path(state.config().resource_path)
                 .join(common::Path("odr_spreadsheet.css")));
    odr::HtmlResource odr_spreadsheet_css_resource = html::HtmlResource::create(
        HtmlResourceType::css, "text/css", "odr_spreadsheet.css",
        "odr_spreadsheet.css", odr_spreadsheet_css_file, true, false, true);
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

  std::string body_clazz = "odr-body";
  if (paged_content) {
    body_clazz += " odr-background";
  }
  if (document.document_type() == DocumentType::spreadsheet) {
    switch (state.config().spreadsheet_gridlines) {
    case HtmlTableGridlines::soft:
      body_clazz += " odr-gridlines-soft";
      break;
    case HtmlTableGridlines::hard:
      body_clazz += " odr-gridlines-hard";
      break;
    case HtmlTableGridlines::none:
    default:
      body_clazz += " odr-gridlines-none";
      break;
    }
  }

  state.out().write_body_begin(HtmlElementOptions().set_class(body_clazz));

  if (paged_content) {
    state.out().write_element_begin(
        "div", HtmlElementOptions().set_class("odr-pages"));
  }
}

void back(const Document &document, const WritingState &state) {
  bool paged_content = ((document.document_type() == DocumentType::text) &&
                        state.config().text_document_margin) ||
                       document.document_type() == DocumentType::presentation ||
                       document.document_type() == DocumentType::drawing;

  if (paged_content) {
    state.out().write_element_end("div");
  }

  auto odr_js_file = File(
      common::Path(state.config().resource_path).join(common::Path("odr.js")));
  odr::HtmlResource odr_js_resource = html::HtmlResource::create(
      HtmlResourceType::js, "text/javascript", "odr.js", "odr.js", odr_js_file,
      true, false, true);
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

std::string get_output_path(const Document &document,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  return fill_path_variables(output_path + "/" +
                             config.document_output_file_name);
}

std::string get_output_path(const Document &document, std::uint32_t index,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  if (document.document_type() == DocumentType::presentation) {
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

class HtmlFragmentBase {
public:
  HtmlFragmentBase(std::string name, Document document)
      : m_name{std::move(name)}, m_document{std::move(document)} {}

  [[nodiscard]] const std::string &name() const { return m_name; }

  virtual void write_fragment(HtmlWriter &out, WritingState &state) const = 0;

  void write_document(HtmlWriter &out, WritingState &state) const {
    front(m_document, state);
    write_fragment(out, state);
    back(m_document, state);
  }

protected:
  std::string m_name;
  Document m_document;
};

class HtmlFragmentView final : public HtmlView {
public:
  HtmlFragmentView(const abstract::HtmlService &service, std::string name,
                   std::string path, std::shared_ptr<HtmlFragmentBase> fragment)
      : HtmlView(service, std::move(name), std::move(path)),
        m_fragment{std::move(fragment)} {}

  HtmlResources write_html(html::HtmlWriter &out) const final {
    HtmlResources resources;
    WritingState state(out, service().config(), service().resource_locator(),
                       resources);
    m_fragment->write_document(out, state);
    return resources;
  }

private:
  std::shared_ptr<HtmlFragmentBase> m_fragment;
};

class HtmlServiceImpl : public HtmlService {
public:
  HtmlServiceImpl(Document document,
                  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments,
                  HtmlConfig config, HtmlResourceLocator resource_locator)
      : HtmlService(std::move(config), std::move(resource_locator)),
        m_document{std::move(document)}, m_fragments{std::move(fragments)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "document", "document.html"));
    for (const auto &fragment : m_fragments) {
      if (fragment->name() == "document") {
        continue;
      }

      m_views.emplace_back(std::make_shared<HtmlFragmentView>(
          *this, fragment->name(), fragment->name() + ".html", fragment));
    }
  }

  const HtmlViews &list_views() const final { return m_views; }

  [[nodiscard]] Document document() const { return m_document; }

  [[nodiscard]] std::vector<std::shared_ptr<HtmlFragmentBase>>
  fragments() const {
    return m_fragments;
  }

  void warmup() const final {
    if (m_warm) {
      return;
    }

    common::NullStream null;
    HtmlWriter out(null, config());
    m_resources = write_document(out);

    m_warm = true;
  }

  bool exists(const std::string &path) const final {
    if (std::ranges::any_of(m_views, [&path](const auto &view) {
          return view.path() == path;
        })) {
      return true;
    }

    warmup();

    if (std::ranges::any_of(m_resources, [&path](const auto &pair) {
          const auto &[resource, location] = pair;
          return location.has_value() && location.value() == path;
        })) {
      return true;
    }

    return false;
  }

  std::string mimetype(const std::string &path) const final {
    if (std::ranges::any_of(m_views, [&path](const auto &view) {
          return view.path() == path;
        })) {
      return "text/html";
    }

    warmup();

    for (const auto &[resource, location] : m_resources) {
      if (location.has_value() && location.value() == path) {
        return resource.mime_type();
      }
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const final {
    for (const auto &view : m_views) {
      if (view.path() == path) {
        HtmlWriter writer(out, config());
        write_html(path, writer);
        return;
      }
    }

    warmup();

    for (const auto &[resource, location] : m_resources) {
      if (location.has_value() && location.value() == path) {
        util::stream::pipe(*resource.file().stream(), out);
        return;
      }
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           html::HtmlWriter &out) const final {
    if (path == "document.html") {
      return write_document(out);
    }

    for (const auto &view : m_views) {
      if (view.path() == path) {
        return view.impl()->write_html(out);
      }
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_document(HtmlWriter &out) const {
    HtmlResources resources;

    WritingState state(out, config(), resource_locator(), resources);

    front(document(), state);
    for (const auto &fragment : m_fragments) {
      fragment->write_fragment(out, state);
    }
    back(document(), state);

    return resources;
  }

protected:
  Document m_document;
  std::vector<std::shared_ptr<HtmlFragmentBase>> m_fragments;

  HtmlViews m_views;

  mutable bool m_warm = false;
  mutable HtmlResources m_resources;
};

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(Document document)
      : HtmlFragmentBase("document", std::move(document)) {}

  void write_fragment(HtmlWriter &out, WritingState &state) const final {
    auto root = m_document.root_element();
    auto element = root.text_root();

    if (state.config().text_document_margin) {
      auto page_layout = element.page_layout();
      page_layout.height = {};

      out.write_element_begin(
          "div", HtmlElementOptions()
                     .set_class("odr-page-outer")
                     .set_style(translate_outer_page_style(page_layout)));
      out.write_element_begin(
          "div", HtmlElementOptions()
                     .set_class("odr-page-inner")
                     .set_style(translate_inner_page_style(page_layout)));

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
  explicit SlideHtmlFragment(Document document, Slide slide)
      : HtmlFragmentBase(slide.name(), std::move(document)), m_slide{slide} {}

  void write_fragment(HtmlWriter &, WritingState &state) const final {
    html::translate_slide(m_slide, state);
  }

private:
  Slide m_slide;
};

class SheetHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SheetHtmlFragment(Document document, Sheet sheet)
      : HtmlFragmentBase(sheet.name(), std::move(document)), m_sheet{sheet} {}

  void write_fragment(HtmlWriter &, WritingState &state) const final {
    translate_sheet(m_sheet, state);
  }

private:
  Sheet m_sheet;
};

class PageHtmlFragment final : public HtmlFragmentBase {
public:
  explicit PageHtmlFragment(Document document, Page page)
      : HtmlFragmentBase(page.name(), std::move(document)), m_page{page} {}

  void write_fragment(HtmlWriter &, WritingState &state) const final {
    html::translate_page(m_page, state);
  }

private:
  Page m_page;
};

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

odr::HtmlService html::create_document_service(const Document &document,
                                               const std::string &output_path,
                                               const HtmlConfig &config) {
  HtmlResourceLocator resource_locator =
      local_resource_locator(output_path, config);

  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments;

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

  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      document, fragments, config, resource_locator));
}

Html html::translate_document(const odr::Document &document,
                              const std::string &output_path,
                              const odr::HtmlConfig &config) {
  odr::HtmlService document_service =
      create_document_service(document, output_path, config);
  auto document_service_impl =
      std::dynamic_pointer_cast<HtmlServiceImpl>(document_service.impl());

  HtmlResources resources;
  std::vector<HtmlPage> pages;

  {
    std::string filled_path = get_output_path(document, output_path, config);
    std::ofstream ostream(filled_path, std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    html::HtmlWriter out(ostream, config.format_html, config.html_indent);

    document_service_impl->write_document(out);

    pages.emplace_back("document", filled_path);
  }

  std::uint32_t i = 0;
  for (const auto &fragment : document_service_impl->fragments()) {
    if (fragment->name() == "document") {
      continue;
    }

    std::string filled_path = get_output_path(document, i, output_path, config);
    std::ofstream ostream(filled_path, std::ios::out);
    if (!ostream.is_open()) {
      throw FileWriteError();
    }
    html::HtmlWriter out(ostream, config.format_html, config.html_indent);

    html::WritingState state(
        out, config, document_service_impl->resource_locator(), resources);
    fragment->write_document(out, state);

    pages.emplace_back(fragment->name(), filled_path);

    ++i;
  }

  return {document.file_type(), config, std::move(pages), document};
}

} // namespace odr::internal
