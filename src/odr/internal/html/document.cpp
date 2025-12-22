#include <odr/internal/html/document.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

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
#include <mutex>

namespace odr::internal::html {
namespace {

void front(const Document &document, const WritingState &state) {
  HtmlWriter &out = state.out();

  const bool paged_content =
      (document.document_type() == DocumentType::text &&
       state.config().text_document_margin) ||
      document.document_type() == DocumentType::presentation ||
      document.document_type() == DocumentType::drawing;

  out.write_begin();
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  if (paged_content) {
    out.write_header_viewport("width=device-width,user-scalable=yes");
  } else {
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
  }

  auto odr_css_file = File(
      AbsPath(state.config().resource_path).join(RelPath("odr.css")).string());
  odr::HtmlResource odr_css_resource =
      HtmlResource::create(HtmlResourceType::css, "text/css", "odr.css",
                           "odr.css", odr_css_file, true, false, true);
  HtmlResourceLocation odr_css_location =
      state.config().resource_locator(odr_css_resource, state.config());
  state.resources().emplace_back(std::move(odr_css_resource), odr_css_location);
  if (odr_css_location.has_value()) {
    out.write_header_style(odr_css_location.value());
  } else {
    out.write_header_style_begin();
    util::stream::pipe(*odr_css_file.stream(), out.out());
    out.write_header_style_end();
  }

  if (document.document_type() == DocumentType::spreadsheet) {
    auto odr_spreadsheet_css_file =
        File(AbsPath(state.config().resource_path)
                 .join(RelPath("odr_spreadsheet.css"))
                 .string());
    odr::HtmlResource odr_spreadsheet_css_resource = HtmlResource::create(
        HtmlResourceType::css, "text/css", "odr_spreadsheet.css",
        "odr_spreadsheet.css", odr_spreadsheet_css_file, true, false, true);
    HtmlResourceLocation odr_spreadsheet_css_location =
        state.config().resource_locator(odr_spreadsheet_css_resource,
                                        state.config());
    state.resources().emplace_back(std::move(odr_spreadsheet_css_resource),
                                   odr_spreadsheet_css_location);
    if (odr_spreadsheet_css_location.has_value()) {
      out.write_header_style(odr_spreadsheet_css_location.value());
    } else {
      out.write_header_style_begin();
      util::stream::pipe(*odr_spreadsheet_css_file.stream(), out.out());
      out.write_header_style_end();
    }
  }

  out.write_header_end();

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

  out.write_body_begin(HtmlElementOptions().set_class(body_clazz));

  if (paged_content) {
    out.write_element_begin("div", HtmlElementOptions().set_class("odr-pages"));
  }
}

void back(const Document &document, const WritingState &state) {
  HtmlWriter &out = state.out();

  const bool paged_content =
      (document.document_type() == DocumentType::text &&
       state.config().text_document_margin) ||
      document.document_type() == DocumentType::presentation ||
      document.document_type() == DocumentType::drawing;

  if (paged_content) {
    out.write_element_end("div");
  }

  auto odr_js_file = File(
      AbsPath(state.config().resource_path).join(RelPath("odr.js")).string());
  odr::HtmlResource odr_js_resource =
      HtmlResource::create(HtmlResourceType::js, "text/javascript", "odr.js",
                           "odr.js", odr_js_file, true, false, true);
  HtmlResourceLocation odr_js_location =
      state.config().resource_locator(odr_js_resource, state.config());
  state.resources().emplace_back(std::move(odr_js_resource), odr_js_location);
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
                                const std::optional<std::uint32_t> index = {}) {
  std::string result = path;
  util::string::replace_all(result, "{index}",
                            index ? std::to_string(*index) : "");
  return result;
}

class HtmlFragmentBase {
public:
  HtmlFragmentBase(std::string name, const std::size_t index, std::string path,
                   Document document)
      : m_name{std::move(name)}, m_index{index}, m_path{std::move(path)},
        m_document{std::move(document)} {}

  virtual ~HtmlFragmentBase() = default;

  [[nodiscard]] const std::string &name() const { return m_name; }
  [[nodiscard]] std::size_t index() const { return m_index; }
  [[nodiscard]] const std::string &path() const { return m_path; }

  virtual void write_fragment(HtmlWriter &out, WritingState &state) const = 0;

  void write_document(HtmlWriter &out, WritingState &state) const {
    front(m_document, state);
    write_fragment(out, state);
    back(m_document, state);
  }

protected:
  std::string m_name;
  std::size_t m_index = 0;
  std::string m_path;
  Document m_document;
};

class HtmlFragmentView final : public abstract::HtmlView {
public:
  HtmlFragmentView(const abstract::HtmlService &service,
                   std::shared_ptr<HtmlFragmentBase> fragment)
      : m_service{&service}, m_fragment{std::move(fragment)} {}

  [[nodiscard]] const std::string &name() const override {
    return m_fragment->name();
  }
  [[nodiscard]] std::size_t index() const override {
    return m_fragment->index();
  }
  [[nodiscard]] const std::string &path() const override {
    return m_fragment->path();
  }
  [[nodiscard]] const HtmlConfig &config() const override {
    return m_service->config();
  }
  [[nodiscard]] const abstract::HtmlService &service() const {
    return *m_service;
  }

  HtmlResources write_html(HtmlWriter &out) const override {
    HtmlResources resources;
    WritingState state(out, service().config(), resources);
    m_fragment->write_document(out, state);
    return resources;
  }

private:
  const abstract::HtmlService *m_service = nullptr;
  std::shared_ptr<HtmlFragmentBase> m_fragment;
};

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(Document document,
                  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments,
                  HtmlConfig config, std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_document{std::move(document)}, m_fragments{std::move(fragments)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "document", 0, "document.html"));
    for (const auto &fragment : m_fragments) {
      if (fragment->name() == "document") {
        continue;
      }
      m_views.emplace_back(std::make_shared<HtmlFragmentView>(*this, fragment));
    }
  }

  const HtmlViews &list_views() const override { return m_views; }

  void warmup() const override {
    std::lock_guard lock(m_mutex);

    if (m_warm) {
      return;
    }

    NullStream null;
    HtmlWriter out(null, config());
    m_resources = write_document(out);

    m_warm = true;
  }

  bool exists(const std::string &path) const override {
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

  std::string mimetype(const std::string &path) const override {
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

  void write(const std::string &path, std::ostream &out) const override {
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
        resource.write_resource(out);
        return;
      }
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
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

    WritingState state(out, config(), resources);

    front(m_document, state);
    for (const auto &fragment : m_fragments) {
      fragment->write_fragment(out, state);
    }
    back(m_document, state);

    return resources;
  }

protected:
  Document m_document;
  std::vector<std::shared_ptr<HtmlFragmentBase>> m_fragments;

  HtmlViews m_views;

  mutable std::mutex m_mutex;
  mutable bool m_warm = false;
  mutable HtmlResources m_resources;
};

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(std::string name, const std::size_t index,
                            std::string path, Document document)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)) {}

  void write_fragment(HtmlWriter &out, WritingState &state) const override {
    const Element root = m_document.root_element();
    const TextRoot element = root.as_text_root();

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
  explicit SlideHtmlFragment(std::string name, const std::size_t index,
                             std::string path, Document document,
                             const Slide &slide)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)),
        m_slide{slide} {}

  void write_fragment(HtmlWriter &, WritingState &state) const override {
    translate_slide(m_slide, state);
  }

private:
  Slide m_slide;
};

class SheetHtmlFragment final : public HtmlFragmentBase {
public:
  explicit SheetHtmlFragment(std::string name, const std::size_t index,
                             std::string path, Document document,
                             const Sheet &sheet)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)),
        m_sheet{sheet} {}

  void write_fragment(HtmlWriter &, WritingState &state) const override {
    translate_sheet(m_sheet, state);
  }

private:
  Sheet m_sheet;
};

class PageHtmlFragment final : public HtmlFragmentBase {
public:
  explicit PageHtmlFragment(std::string name, const std::size_t index,
                            std::string path, Document document,
                            const Page &page)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)),
        m_page{page} {}

  void write_fragment(HtmlWriter &, WritingState &state) const override {
    translate_page(m_page, state);
  }

private:
  Page m_page;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

odr::HtmlService html::create_document_service(
    const Document &document, const std::string & /*cache_path*/,
    HtmlConfig config, std::shared_ptr<Logger> logger) {
  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments;

  if (document.document_type() == DocumentType::text) {
    fragments.push_back(std::make_unique<TextHtmlFragment>(
        "document", 0, config.document_output_file_name, document));
  } else if (document.document_type() == DocumentType::presentation) {
    std::size_t i = 0;
    for (Element child : document.root_element().children()) {
      Slide slide = child.as_slide();
      fragments.push_back(std::make_unique<SlideHtmlFragment>(
          slide.name(), i + 1,
          fill_path_variables(config.slide_output_file_name, i), document,
          slide));
      ++i;
    }
  } else if (document.document_type() == DocumentType::spreadsheet) {
    std::size_t i = 0;
    for (Element child : document.root_element().children()) {
      Sheet sheet = child.as_sheet();
      fragments.push_back(std::make_unique<SheetHtmlFragment>(
          sheet.name(), i + 1,
          fill_path_variables(config.sheet_output_file_name, i), document,
          sheet));
      ++i;
    }
  } else if (document.document_type() == DocumentType::drawing) {
    std::size_t i = 0;
    for (Element child : document.root_element().children()) {
      Page page = child.as_page();
      fragments.push_back(std::make_unique<PageHtmlFragment>(
          page.name(), i + 1,
          fill_path_variables(config.page_output_file_name, i), document,
          page));
      ++i;
    }
  } else {
    throw UnknownDocumentType();
  }

  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      document, fragments, std::move(config), std::move(logger)));
}

} // namespace odr::internal
