#include <odr/internal/html/document.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_element.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <mutex>

namespace odr::internal::html {
namespace {

/// Whether the document renders as fixed-size pages on a backdrop rather than
/// reflowing to the viewport.
bool is_paged_content(const Document &document, const HtmlConfig &config) {
  return (document.document_type() == DocumentType::text &&
          config.text_document_margin) ||
         document.document_type() == DocumentType::presentation ||
         document.document_type() == DocumentType::drawing;
}

/// A page box plus the gutters the column puts around it, in css pixels.
std::optional<double> page_content_pixels(const PageLayout &page_layout,
                                          const HtmlConfig &config) {
  const std::optional<double> width = css_pixels(page_layout.width);
  if (!width.has_value()) {
    return {};
  }
  return *width + page_column_gutter_pixels(config);
}

/// Per view, so slides of differing width are each fitted to their own page.
std::optional<double> fragment_content_pixels(const TextRoot &element,
                                              const HtmlConfig &config) {
  return page_content_pixels(element.page_layout(), config);
}
std::optional<double> fragment_content_pixels(const Slide &element,
                                              const HtmlConfig &config) {
  return page_content_pixels(element.page_layout(), config);
}
std::optional<double> fragment_content_pixels(const Page &element,
                                              const HtmlConfig &config) {
  return page_content_pixels(element.page_layout(), config);
}
/// A sheet reflows; there is no page box to fit.
std::optional<double> fragment_content_pixels(const Sheet &,
                                              const HtmlConfig &) {
  return {};
}

/// Only a sheet has cells a limit can cut.
std::optional<HtmlSheetCut> fragment_sheet_cut(const Sheet &sheet,
                                               const HtmlConfig &config) {
  return sheet_cut(sheet, config);
}
std::optional<HtmlSheetCut> fragment_sheet_cut(const Slide &,
                                               const HtmlConfig &) {
  return {};
}
std::optional<HtmlSheetCut> fragment_sheet_cut(const Page &,
                                               const HtmlConfig &) {
  return {};
}

/// The widest of them, for the view that writes every page into one file.
std::optional<double> document_content_pixels(const Document &document,
                                              const HtmlConfig &config) {
  const Element root = document.root_element();

  const auto widest = [](const std::optional<double> lhs,
                         const std::optional<double> rhs) {
    if (!lhs.has_value()) {
      return rhs;
    }
    return rhs.has_value() ? std::optional(std::max(*lhs, *rhs)) : lhs;
  };

  std::optional<double> result;
  switch (document.document_type()) {
  case DocumentType::text:
    result = fragment_content_pixels(root.as_text_root(), config);
    break;
  case DocumentType::presentation:
    for (const Element child : root.children()) {
      result =
          widest(result, fragment_content_pixels(child.as_slide(), config));
    }
    break;
  case DocumentType::drawing:
    for (const Element child : root.children()) {
      result = widest(result, fragment_content_pixels(child.as_page(), config));
    }
    break;
  default:
    break;
  }

  return result;
}

/// A spreadsheet states its direction per table, which is not read yet.
TextDirection document_direction(const Document &document) {
  const Element root = document.root_element();

  std::optional<TextDirection> result;
  switch (document.document_type()) {
  case DocumentType::text:
    result = root.as_text_root().page_layout().direction;
    break;
  case DocumentType::presentation:
    for (const Element child : root.children()) {
      result = child.as_slide().page_layout().direction;
      if (result.has_value()) {
        break;
      }
    }
    break;
  case DocumentType::drawing:
    for (const Element child : root.children()) {
      result = child.as_page().page_layout().direction;
      if (result.has_value()) {
        break;
      }
    }
    break;
  default:
    break;
  }

  return result.value_or(TextDirection::left_to_right);
}

/// A spreadsheet answers the viewport question with its own mode.
std::optional<HtmlViewportMode>
viewport_mode_override(const Document &document, const HtmlConfig &config) {
  return document.document_type() == DocumentType::spreadsheet
             ? config.spreadsheet_viewport_mode
             : std::nullopt;
}

/// @p name titles the view; empty when the whole document is written as one
/// file, which no one view names.
void front(const Document &document, WritingState &state,
           const std::string &name,
           const std::optional<double> content_pixels) {
  HtmlWriter &out = state.out();

  const bool paged_content = is_paged_content(document, state.config());

  state.set_direction(document_direction(document));

  out.write_begin(HtmlElementOptions().set_attributes(HtmlAttributesVector{
      {"dir", translate_text_direction(state.direction())}}));
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_title(
      document.document_type() == DocumentType::spreadsheet && !name.empty()
          ? escape_text(name)
          : "odr");
  const std::optional<HtmlViewportMode> mode_override =
      viewport_mode_override(document, state.config());
  write_viewport_meta(out, state.config(), paged_content, mode_override);
  write_zoom_style(out, state.config(),
                   paged_content
                       ? width_fit(state.config(), paged_content, mode_override)
                       : WidthFit::none,
                   content_pixels);
  write_content_margin_style(out, state.config());

  write_document_style(state);
  write_document_dark_style(state);
  write_search_style(state);
  write_search_dark_style(state);
  if (document.document_type() == DocumentType::spreadsheet) {
    write_spreadsheet_style(state);
    write_spreadsheet_dark_style(state);
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

  if (is_paged_content(document, state.config())) {
    out.write_element_end("div");
  }

  write_search_script(state);
  write_document_script(state);
  if (document.document_type() == DocumentType::spreadsheet) {
    write_spreadsheet_script(state);
  }
  write_viewport_script(state);

  out.write_body_end();
  out.write_end();
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

  /// The width this one view lays out, which is what it is fitted against.
  [[nodiscard]] virtual std::optional<double>
  content_pixels(const HtmlConfig &config) const = 0;

  /// Measured on demand: a walk over the cells, and most hosts never ask.
  [[nodiscard]] const std::optional<HtmlSheetCut> &
  sheet_cut(const HtmlConfig &config) const {
    std::lock_guard lock(m_cut_mutex);
    if (!m_cut_measured) {
      m_cut = measure_sheet_cut(config);
      m_cut_measured = true;
    }
    return m_cut;
  }

  void write_document(HtmlWriter &out, WritingState &state) const {
    const std::optional<double> content = content_pixels(state.config());
    front(m_document, state, m_name, content);
    write_fragment(out, state);
    back(m_document, state);
  }

protected:
  [[nodiscard]] virtual std::optional<HtmlSheetCut>
  measure_sheet_cut(const HtmlConfig &config) const = 0;

  std::string m_name;
  std::size_t m_index = 0;
  std::string m_path;
  Document m_document;

private:
  mutable std::mutex m_cut_mutex;
  mutable std::optional<HtmlSheetCut> m_cut;
  mutable bool m_cut_measured = false;
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
  [[nodiscard]] const std::optional<HtmlSheetCut> &sheet_cut() const override {
    return m_fragment->sheet_cut(config());
  }
  [[nodiscard]] const abstract::HtmlService &service() const {
    return *m_service;
  }

  HtmlResources write_html(HtmlWriter &out) const override {
    HtmlResources resources;
    WritingState state(out, service().config(), resources, service().logger());
    m_fragment->write_document(out, state);
    return resources;
  }

private:
  const abstract::HtmlService *m_service = nullptr;
  std::shared_ptr<HtmlFragmentBase> m_fragment;
};

/// The view that writes every fragment into one file; for a workbook that is
/// every sheet, so it answers for the first one it cut.
class HtmlDocumentView final : public HtmlView {
public:
  HtmlDocumentView(
      const abstract::HtmlService &service, std::string name,
      const std::size_t index, std::string path,
      const std::vector<std::shared_ptr<HtmlFragmentBase>> &fragments)
      : HtmlView(service, std::move(name), index, std::move(path)),
        m_fragments{&fragments} {}

  [[nodiscard]] const std::optional<HtmlSheetCut> &sheet_cut() const override {
    for (const auto &fragment : *m_fragments) {
      if (const std::optional<HtmlSheetCut> &cut =
              fragment->sheet_cut(config());
          cut.has_value()) {
        return cut;
      }
    }
    return HtmlView::sheet_cut();
  }

private:
  const std::vector<std::shared_ptr<HtmlFragmentBase>> *m_fragments{nullptr};
};

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(Document document,
                  std::vector<std::shared_ptr<HtmlFragmentBase>> fragments,
                  HtmlConfig config, const Logger &logger)
      : HtmlService(std::move(config), logger), m_document{std::move(document)},
        m_fragments{std::move(fragments)} {
    m_views.emplace_back(std::make_shared<HtmlDocumentView>(
        *this, "document", 0, "document.html", m_fragments));
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

    return resource_at(m_resources, path) != nullptr;
  }

  std::string mimetype(const std::string &path) const override {
    if (std::ranges::any_of(m_views, [&path](const auto &view) {
          return view.path() == path;
        })) {
      return "text/html";
    }

    warmup();

    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
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

    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      resource->write_resource(out);
      return;
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

    WritingState state(out, config(), resources, logger());

    // every page in one file, so the column is as wide as the widest of them
    const std::optional<double> content =
        document_content_pixels(m_document, config());

    front(m_document, state, "", content);
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

/// Only a paragraph carries a break: one inside a table or a frame could not
/// split the page box.
bool breaks_page(const std::optional<BreakType> &break_type) {
  return break_type == BreakType::page;
}

bool breaks_page_before(const Element &element) {
  return element.type() == ElementType::paragraph &&
         breaks_page(element.as_paragraph().style().break_before);
}

bool breaks_page_after(const Element &element) {
  return element.type() == ElementType::paragraph &&
         breaks_page(element.as_paragraph().style().break_after);
}

class TextHtmlFragment final : public HtmlFragmentBase {
public:
  explicit TextHtmlFragment(std::string name, const std::size_t index,
                            std::string path, Document document)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)) {}

  [[nodiscard]] std::optional<double>
  content_pixels(const HtmlConfig &config) const override {
    return fragment_content_pixels(m_document.root_element().as_text_root(),
                                   config);
  }

  void write_fragment(HtmlWriter &out, WritingState &state) const override {
    const Element root = m_document.root_element();
    const TextRoot element = root.as_text_root();

    if (state.config().text_document_margin) {
      write_pages(out, state, element);
    } else {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class("odr-text-flow"));
      translate_children(element.children(), state);
      out.write_element_end("div");
    }
  }

private:
  /// One page box per run of content between the author's manual breaks. Not
  /// pagination: nothing computes where a page ends, so the boxes differ in
  /// height. `.odr-pages` stacks them.
  static void write_pages(HtmlWriter &out, const WritingState &state,
                          const TextRoot &element) {
    const PageLayout page_layout = element.page_layout();

    const auto begin_page = [&] {
      out.write_element_begin(
          "div",
          HtmlElementOptions()
              .set_class("odr-page-outer")
              .set_style(translate_outer_flowing_page_style(page_layout)));
      out.write_element_begin(
          "div", HtmlElementOptions()
                     .set_class("odr-page-inner")
                     .set_style(translate_inner_page_style(page_layout)));
    };
    const auto end_page = [&] {
      out.write_element_end("div");
      out.write_element_end("div");
    };

    begin_page();
    // Otherwise a leading or doubled break opens an empty sheet.
    bool empty = true;
    bool pending_break = false;

    for (const Element child : element.children()) {
      if (child.type() == ElementType::page_break) {
        // The node is the break, not content.
        pending_break = true;
        continue;
      }
      if ((pending_break || breaks_page_before(child)) && !empty) {
        end_page();
        begin_page();
        empty = true;
      }
      pending_break = breaks_page_after(child);
      translate_element(child, state);
      empty = false;
    }

    end_page();
  }

protected:
  /// A text document has no sheet to cut.
  [[nodiscard]] std::optional<HtmlSheetCut>
  measure_sheet_cut(const HtmlConfig &) const override {
    return {};
  }
};

/// A fragment rendering one top-level element handle (slide, sheet, page)
/// through its `translate_*` function.
template <typename Handle,
          void (*Translate)(const Handle &, const WritingState &)>
class ElementHtmlFragment final : public HtmlFragmentBase {
public:
  explicit ElementHtmlFragment(std::string name, const std::size_t index,
                               std::string path, Document document,
                               const Handle &element)
      : HtmlFragmentBase(std::move(name), index, std::move(path),
                         std::move(document)),
        m_element{element} {}

  [[nodiscard]] std::optional<double>
  content_pixels(const HtmlConfig &config) const override {
    return fragment_content_pixels(m_element, config);
  }

  void write_fragment(HtmlWriter &, WritingState &state) const override {
    Translate(m_element, state);
  }

protected:
  [[nodiscard]] std::optional<HtmlSheetCut>
  measure_sheet_cut(const HtmlConfig &config) const override {
    return fragment_sheet_cut(m_element, config);
  }

private:
  Handle m_element;
};

using SlideHtmlFragment = ElementHtmlFragment<Slide, translate_slide>;
using SheetHtmlFragment = ElementHtmlFragment<Sheet, translate_sheet>;
using PageHtmlFragment = ElementHtmlFragment<Page, translate_page>;

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_document_service(const Document &document,
                                          HtmlConfig config,
                                          const Logger &logger) {
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
      document, fragments, std::move(config), logger));
}

} // namespace odr::internal
