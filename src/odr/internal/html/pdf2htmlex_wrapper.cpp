#include <odr/internal/html/pdf2htmlex_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/global_params.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <pdf2htmlEX/HTMLRenderer/HTMLRenderer.h>
#include <pdf2htmlEX/Param.h>

#include <poppler/PDFDoc.h>

#include <algorithm>

namespace odr::internal::html {

namespace {

pdf2htmlEX::Param create_params(PDFDoc &pdf_doc, const HtmlConfig &config,
                                const std::string &output_path) {
  (void)config;

  pdf2htmlEX::Param param;

  // pages
  param.first_page = 1;
  param.last_page = pdf_doc.getNumPages();

  // dimension
  param.zoom = 0;
  param.fit_width = 0;
  param.fit_height = 0;
  param.use_cropbox = 1;
  param.desired_dpi = 144;

  // output
  param.embed_css = 1;
  param.embed_font = 1;
  param.embed_image = 1;
  param.embed_javascript = 1;
  param.embed_outline = 1;
  param.split_pages = 0;
  param.dest_dir = output_path;
  param.css_filename = "style.css";
  param.page_filename = "page%i.html";
  param.outline_filename = "outline.html";
  param.process_nontext = 1;
  param.process_outline = 1;
  param.process_annotation = 0;
  param.process_form = 0;
  param.printing = 1;
  param.fallback = 0;
  param.tmp_file_size_limit = -1;
  param.delay_background = 0;

  // font
  param.embed_external_font = 0; // TODO 1
  param.font_format = "woff";
  param.decompose_ligature = 0;
  param.turn_off_ligatures = 0;
  param.auto_hint = 0;
  param.external_hint_tool = "";
  param.stretch_narrow_glyph = 0;
  param.squeeze_wide_glyph = 1;
  param.override_fstype = 0;
  param.process_type3 = 0;

  // text
  param.h_eps = 1.0;
  param.v_eps = 1.0;
  param.space_threshold = 1.0 / 8;
  param.font_size_multiplier = 4.0;
  param.space_as_offset = 0;
  param.tounicode = 0;
  param.optimize_text = 0;
  param.correct_text_visibility = 1;
  param.text_dpi = 300;

  // background
  param.bg_format = "png";
  param.svg_node_count_limit = -1;
  param.svg_embed_bitmap = 1;

  // encryption
  param.owner_password = "";
  param.user_password = "";
  param.no_drm = 0;

  // misc
  param.clean_tmp = 1;
  param.tmp_dir = output_path;
  param.data_dir = odr::GlobalParams::pdf2htmlex_data_path();
  param.poppler_data_dir = odr::GlobalParams::poppler_data_path();
  param.debug = 0;
  param.proof = 0;
  param.quiet = 1;

  // input, output
  param.input_filename = "";
  param.output_filename = "document.html";

  return param;
}

} // namespace

class BackgroundImageResource : public HtmlResource {
public:
  static std::string file_name(std::size_t page_number,
                               const std::string &format) {
    std::stringstream stream;
    stream << "bg";
    stream << std::hex << page_number;
    stream << "." << format;
    return stream.str();
  }

  BackgroundImageResource(
      PopplerPdfFile pdf_file, const std::string &output_path,
      std::shared_ptr<pdf2htmlEX::HTMLRenderer> html_renderer,
      std::shared_ptr<std::mutex> html_renderer_mutex,
      std::shared_ptr<pdf2htmlEX::Param> html_renderer_param, int page_number,
      const std::string &format)
      : HtmlResource(HtmlResourceType::image, "image/jpg",
                     file_name(page_number, format),
                     output_path + "/" + file_name(page_number, format),
                     odr::File(), false, false, true),
        m_pdf_file{std::move(pdf_file)},
        m_html_renderer{std::move(html_renderer)},
        m_html_renderer_mutex{std::move(html_renderer_mutex)},
        m_html_renderer_param{std::move(html_renderer_param)},
        m_page_number{page_number} {}

  void warmup() const {
    PDFDoc &pdf_doc = m_pdf_file.pdf_doc();

    std::lock_guard lock(m_mutex);

    if (!std::filesystem::exists(path())) {
      std::lock_guard renderer_lock(*m_html_renderer_mutex);

      m_html_renderer->renderPage(&pdf_doc, m_page_number);
    }
  }

  void write_resource(std::ostream &os) const override {
    warmup();

    std::ifstream in(path());
    if (!in.is_open()) {
      throw FileWriteError();
    }
    util::stream::pipe(in, os);
  }

private:
  PopplerPdfFile m_pdf_file;
  std::shared_ptr<pdf2htmlEX::HTMLRenderer> m_html_renderer;
  std::shared_ptr<std::mutex> m_html_renderer_mutex;
  std::shared_ptr<pdf2htmlEX::Param> m_html_renderer_param;
  int m_page_number;
  mutable std::mutex m_mutex;
};

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(PopplerPdfFile pdf_file, std::string output_path,
                  std::shared_ptr<pdf2htmlEX::HTMLRenderer> html_renderer,
                  std::shared_ptr<std::mutex> html_renderer_mutex,
                  std::shared_ptr<pdf2htmlEX::Param> html_renderer_param,
                  HtmlConfig config, HtmlResourceLocator resource_locator)
      : HtmlService(std::move(config), std::move(resource_locator)),
        m_pdf_file{std::move(pdf_file)}, m_output_path{std::move(output_path)},
        m_html_renderer{std::move(html_renderer)},
        m_html_renderer_mutex{std::move(html_renderer_mutex)},
        m_html_renderer_param{std::move(html_renderer_param)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "document", "document.html"));
  }

  const HtmlViews &list_views() const final { return m_views; }

  void warmup() const final {
    if (m_warm) {
      return;
    }

    for (int i = 1; i <= m_pdf_file.pdf_doc().getNumPages(); ++i) {
      auto resource = std::make_shared<BackgroundImageResource>(
          m_pdf_file, m_output_path, m_html_renderer, m_html_renderer_mutex,
          m_html_renderer_param, i, m_html_renderer_param->bg_format);
      std::string file_name = BackgroundImageResource::file_name(
          i, m_html_renderer_param->bg_format);
      m_resources.emplace_back(std::move(resource), std::move(file_name));
    }

    m_warm = true;
  }

  bool exists(const std::string &path) const final {
    if (std::ranges::any_of(m_views, [&path](const auto &view) {
          return view.path() == path;
        })) {
      return true;
    }

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

    for (const auto &[resource, location] : m_resources) {
      if (location.has_value() && location.value() == path) {
        return resource.mime_type();
      }
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_document(HtmlWriter &out) const {
    warmup();

    std::ifstream in(m_output_path + "/document.html");
    util::stream::pipe(in, out.out());

    return m_resources;
  }

  void write(const std::string &path, std::ostream &out) const final {
    warmup();

    for (const auto &view : m_views) {
      if (view.path() == path) {
        HtmlWriter writer(out, config());
        write_html(path, writer);
        return;
      }
    }

    for (const auto &[resource, location] : m_resources) {
      if (location.has_value() && location.value() == path) {
        resource.write_resource(out);
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

    throw FileNotFound("Unknown path: " + path);
  }

private:
  PopplerPdfFile m_pdf_file;
  std::string m_output_path;
  std::shared_ptr<pdf2htmlEX::HTMLRenderer> m_html_renderer;
  std::shared_ptr<std::mutex> m_html_renderer_mutex;
  std::shared_ptr<pdf2htmlEX::Param> m_html_renderer_param;

  HtmlViews m_views;

  mutable bool m_warm = false;
  mutable HtmlResources m_resources;
};

} // namespace odr::internal::html

namespace odr::internal {

odr::HtmlService
html::create_poppler_pdf_service(const PopplerPdfFile &pdf_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config) {
  PDFDoc &pdf_doc = pdf_file.pdf_doc();

  auto html_renderer_param = std::make_shared<pdf2htmlEX::Param>(
      create_params(pdf_doc, config, output_path));
  html_renderer_param->embed_image = 0;
  html_renderer_param->delay_background = 1;

  if (!pdf_doc.okToCopy()) {
    if (html_renderer_param->no_drm == 0) {
      throw odr::DocumentCopyProtectedException();
    }
  }

  // TODO not sure what the `progPath` is used for. it cannot be `nullptr`
  // TODO potentially just a cache dir?
  auto html_renderer = std::make_shared<pdf2htmlEX::HTMLRenderer>(
      odr::GlobalParams::fontconfig_data_path().c_str(), *html_renderer_param);
  html_renderer->process(&pdf_doc);

  HtmlResourceLocator resource_locator =
      local_resource_locator(output_path, config);

  // renderer is not thread safe
  // TODO check if this can be achieved in pdf2htmlEX
  auto html_renderer_mutex = std::make_shared<std::mutex>();

  return odr::HtmlService(std::make_shared<HtmlServiceImpl>(
      pdf_file, output_path, std::move(html_renderer),
      std::move(html_renderer_mutex), std::move(html_renderer_param), config,
      resource_locator));
}

} // namespace odr::internal
