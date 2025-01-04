#include <odr/internal/html/pdf2htmlex_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <pdf2htmlEX/HTMLRenderer/HTMLRenderer.h>
#include <pdf2htmlEX/Param.h>

#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>

namespace odr::internal::html {

namespace {

pdf2htmlEX::Param create_params(PDFDoc &pdf_doc, const HtmlConfig &config,
                                const std::string &output_path) {
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
  param.embed_image = 0;
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
  param.delay_background = 1;

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
  param.data_dir = config.pdf2htmlex_data_path;
  param.poppler_data_dir = config.poppler_data_path;
  param.debug = 0;
  param.proof = 0;
  param.quiet = 1;

  // input, output
  param.input_filename = "";
  param.output_filename = "document.html";

  return param;
}

} // namespace

class StaticHtmlService : public abstract::HtmlService {
public:
  StaticHtmlService(
      PopplerPdfFile pdf_file,
      std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments)
      : m_pdf_file{std::move(pdf_file)}, m_fragments{std::move(fragments)} {}

  [[nodiscard]] const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
  fragments() const override {
    return m_fragments;
  }

  void write_html_document(
      HtmlWriter &out, const HtmlConfig &config,
      const HtmlResourceLocator &resourceLocator) const override {}

private:
  PopplerPdfFile m_pdf_file;
  const std::vector<std::shared_ptr<abstract::HtmlFragment>> m_fragments;
};

} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::translate_poppler_pdf_file(const PopplerPdfFile &pdf_file) {
  return HtmlService(nullptr);
}

Html html::translate_poppler_pdf_file(const PopplerPdfFile &pdf_file,
                                      const std::string &output_path,
                                      const HtmlConfig &config) {
  PDFDoc &pdf_doc = pdf_file.pdf_doc();

  pdf2htmlEX::Param param = create_params(pdf_doc, config, output_path);

  if (!pdf_doc.okToCopy()) {
    if (param.no_drm == 0) {
      throw DocumentCopyProtectedException("");
    }
  }

  globalParams = std::make_unique<GlobalParams>(
      !param.poppler_data_dir.empty() ? param.poppler_data_dir.c_str()
                                      : nullptr);

  // TODO not sure what the `progPath` is used for. it cannot be `nullptr`
  // TODO potentially just a cache dir?
  pdf2htmlEX::HTMLRenderer html_renderer(config.fontforge_data_path.c_str(),
                                         param);
  html_renderer.process(&pdf_doc);
  if (param.delay_background != 0) {
    for (int i = 1; i <= pdf_doc.getNumPages(); ++i) {
      html_renderer.renderPage(&pdf_doc, i);
    }
  }

  globalParams.reset();

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/document.html"}}};
}

} // namespace odr::internal
