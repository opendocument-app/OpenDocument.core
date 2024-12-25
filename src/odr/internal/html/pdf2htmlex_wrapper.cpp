#include <odr/internal/html/pdf2htmlex_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
#include <odr/internal/project_info.hpp>

#include <pdf2htmlEX/HTMLRenderer/HTMLRenderer.h>
#include <pdf2htmlEX/Param.h>

#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>

namespace odr::internal {

Html html::translate_poppler_pdf_file(const PopplerPdfFile &pdf_file,
                                      const std::string &output_path,
                                      const HtmlConfig &config) {
  PDFDoc &pdf_doc = pdf_file.pdf_doc();

  const char *fontconfig_path = std::getenv("FONTCONFIG_PATH");
  if (fontconfig_path == nullptr) {
    // Storage is allocated and after successful putenv, it will never be freed.
    // This is the way of putenv.
    char *storage = strdup("FONTCONFIG_PATH=" FONTCONFIG_PATH);
    if (0 != putenv(storage)) {
      free(storage);
    }
    fontconfig_path = std::getenv("FONTCONFIG_PATH");
  }

  pdf2htmlEX::Param param;

  // pages
  param.first_page = 1;
  param.last_page = pdf_doc.getNumPages();

  // dimension
  param.zoom = 0;
  param.fit_width = 0;
  param.fit_height = 0;
  param.use_cropbox = 1;
  param.desired_dpi = 144.0;

  // output
  param.embed_css = 1;
  param.embed_font = 1;
  param.embed_image = 1;
  param.embed_javascript = 1;
  param.embed_outline = 1;
  param.split_pages = 0;
  param.dest_dir = output_path;
  param.css_filename = "";
  param.page_filename = "";
  param.outline_filename = "";
  param.process_nontext = 1;
  param.process_outline = 1;
  param.process_annotation = 0;
  param.process_form = 0;
  param.printing = 1;
  param.fallback = 0;
  param.tmp_file_size_limit = -1;

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
  param.tmp_dir = "/tmp";
  param.data_dir = PDF2HTMLEX_DATA_DIR;
  param.poppler_data_dir = POPPLER_DATA_DIR;
  param.debug = 0;
  param.proof = 0;
  param.quiet = 1;

  // input, output
  param.input_filename = "";
  param.output_filename = "document.html";

  if (!pdf_doc.okToCopy()) {
    if (param.no_drm == 0) {
      throw DocumentCopyProtectedException("");
    }
  }

  globalParams = std::make_unique<GlobalParams>(
      !param.poppler_data_dir.empty() ? param.poppler_data_dir.c_str()
                                      : nullptr);

  pdf2htmlEX::HTMLRenderer(fontconfig_path, param).process(&pdf_doc);

  globalParams.reset();

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/document.html"}}};
}

} // namespace odr::internal
