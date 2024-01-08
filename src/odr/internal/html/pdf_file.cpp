#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <fstream>

namespace odr::internal {

Html html::translate_pdf_file(const PdfFile &pdf_file,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  auto in = pdf_file.file().stream();
  pdf::DocumentParser parser(*in);

  std::unique_ptr<pdf::Document> document = parser.parse_document();

  std::vector<pdf::Page *> ordered_pages;
  std::function<void(pdf::Pages * pages)> recurse_pages =
      [&](pdf::Pages *pages) {
        for (pdf::Element *kid : pages->kids) {
          if (auto p = dynamic_cast<pdf::Pages *>(kid); p != nullptr) {
            recurse_pages(p);
          } else if (auto page = dynamic_cast<pdf::Page *>(kid);
                     page != nullptr) {
            ordered_pages.push_back(page);
          } else {
            throw std::runtime_error("unhandled element");
          }
        }
      };

  recurse_pages(document->catalog->pages);

  auto output_file_path = output_path + "/document.html";
  std::ofstream ostream(output_file_path);
  if (!ostream.is_open()) {
    throw FileWriteError();
  }
  HtmlWriter out(ostream, config.format_html, config.html_indent);

  out.write_begin();
  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  out.write_header_viewport(
      "width=device-width,initial-scale=1.0,user-scalable=yes");
  out.write_header_end();

  out.write_body_begin();

  for (pdf::Page *page : ordered_pages) {
    pdf::Array page_box = page->object.as_dictionary()["MediaBox"].as_array();

    out.write_element_begin(
        "div", HtmlElementOptions().set_style([&](std::ostream &o) {
          o << "position:relative;";
          o << "width:" << page_box[2].as_real() / 72.0 << "in;";
          o << "height:" << page_box[3].as_real() / 72.0 << "in;";
        }));

    pdf::IndirectObject page_contents_object =
        parser.read_object(page->contents_reference);
    std::string stream = parser.read_object_stream(page_contents_object);
    std::string page_content = crypto::util::zlib_inflate(stream);

    std::istringstream ss(page_content);
    pdf::GraphicsOperatorParser parser2(ss);
    pdf::GraphicsState state;
    while (!ss.eof()) {
      pdf::GraphicsOperator op = parser2.read_operator();
      state.execute(op);

      if (op.type == pdf::GraphicsOperatorType::text_next_line) {
        double leading = state.current().text.leading;
        double size = state.current().text.size;

        state.current().text.offset[1] -= size + leading;
      } else if (op.type == pdf::GraphicsOperatorType::show_text) {
        const std::string &font_ref = state.current().text.font;
        double size = state.current().text.size;

        std::array<double, 2> offset = state.current().text.offset;

        pdf::Font *font = page->resources->font.at(font_ref);

        const std::string &glyphs = op.arguments[0].as_string();
        std::string unicode = font->cmap.translate_string(glyphs);

        if (unicode.find("Colored Line") != std::string::npos) {
          std::cout << "hi" << std::endl;
        }

        out.write_element_begin(
            "span", HtmlElementOptions().set_style([&](std::ostream &o) {
              o << "position:absolute;";
              o << "left:" << offset[0] / 72.0 << "in;";
              o << "bottom:" << offset[1] / 72.0 << "in;";
              o << "font-size:" << size << "pt;";
            }));
        out.write_raw(unicode);
        out.write_element_end("span");
      } else if (op.type ==
                 pdf::GraphicsOperatorType::show_text_manual_spacing) {
        const std::string &font_ref = state.current().text.font;
        pdf::Font *font = page->resources->font.at(font_ref);
        double size = state.current().text.size;

        std::cout << font->object << std::endl;

        for (const auto &element : op.arguments[0].as_array()) {
          if (element.is_real()) {
            std::cout << "spacing: " << element.as_real() << std::endl;
          } else if (element.is_string()) {
            const std::string &glyphs = element.as_string();
            std::string unicode = font->cmap.translate_string(glyphs);
            std::cout << "show text manual spacing: font=" << font
                      << ", size=" << size << ", text=" << unicode << std::endl;
          }
        }
      } else if (op.type == pdf::GraphicsOperatorType::show_text_next_line) {
        std::cout << "TODO show_text_next_line" << std::endl;
      } else if (op.type ==
                 pdf::GraphicsOperatorType::show_text_next_line_set_spacing) {
        std::cout << "TODO show_text_next_line_set_spacing" << std::endl;
      }
    }

    out.write_element_end("div");
  }

  out.write_body_end();
  out.write_end();

  return {FileType::portable_document_format,
          config,
          {{"document", output_file_path}}};
}

} // namespace odr::internal
