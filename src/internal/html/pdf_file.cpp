#include <internal/html/pdf_file.h>
#include <internal/pdf/pdf_file.h>
#include <odr/html.h>
#include <pdf2htmlEX/HTMLRenderer/HTMLRenderer.h>
#include <pdf2htmlEX/Param.h>
#include <poppler/GlobalParams.h>

class PDFDoc;

namespace odr::internal {

namespace {
pdf2htmlEX::Param get_params(PDFDoc *doc, const std::string &output_directory,
                             const std::string &filename,
                             const HtmlConfig &config) {
  pdf2htmlEX::Param param;

  param.input_filename = "";
  param.output_filename = filename;

  // pages
  param.first_page = 1;
  param.last_page = doc->getNumPages();

  // dimensions
  param.zoom = 0;
  param.fit_width = 0;
  param.fit_height = 0;
  param.use_cropbox = 1;
  param.desired_dpi = 144;

  // output files
  param.embed_css = 1;
  param.embed_font = 1;
  param.embed_image = 1;
  param.embed_javascript = 1;
  param.embed_outline = 1;
  param.split_pages = 0;
  param.dest_dir = output_directory;
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

  // fonts
  param.embed_external_font = 1;
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

  // background image
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
  param.data_dir =
      "/home/andreas/.conan/data/pdf2htmlEX/0.18.8-rc1/_/_/package/"
      "f70383f3ab920ca8d5e7cc155ea235678722dd15/share/pdf2htmlEX";
  param.poppler_data_dir = ""; // TODO
  param.debug = 0;
  param.proof = 0;
  param.quiet = 0;

  return param;
}
} // namespace

Html html::translate_pdf_file(const PdfFile &pdf_file, const std::string &path,
                              const HtmlConfig &config) {
  auto doc = pdf_file.impl()->doc();

  auto output_path = path + "/document.html";
  auto param = get_params(doc, path, "document.html", config);

  globalParams = std::make_unique<GlobalParams>();

  std::make_unique<pdf2htmlEX::HTMLRenderer>("/tmp", param)->process(doc);

  return {pdf_file.file_type(), config, {{"document", output_path}}};
}

} // namespace odr::internal
