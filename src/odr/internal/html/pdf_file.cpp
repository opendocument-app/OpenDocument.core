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

      const std::string &font = state.current().text.font;
      double size = state.current().text.size;

      if (op.type == pdf::GraphicsOperatorType::show_text) {
        const std::string &glyphs = op.arguments[0].as_string();
        std::string unicode =
            page->resources->font.at(font)->cmap.translate_string(glyphs);
        std::cout << "show text: font=" << font << ", size=" << size
                  << ", text=" << unicode << std::endl;
      } else if (op.type ==
                 pdf::GraphicsOperatorType::show_text_manual_spacing) {
        for (const auto &element : op.arguments[0].as_array()) {
          if (element.is_real()) {
            std::cout << "spacing: " << element.as_real() << std::endl;
          } else if (element.is_string()) {
            const std::string &glyphs = element.as_string();
            std::string unicode =
                page->resources->font.at(font)->cmap.translate_string(glyphs);
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
  }

  out.write_body_end();
  out.write_end();

  return {FileType::portable_document_format,
          config,
          {{"document", output_file_path}}};
}

} // namespace odr::internal
