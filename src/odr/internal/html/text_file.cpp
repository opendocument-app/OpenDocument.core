#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
#include <sstream>

namespace odr::internal {

Html html::translate_text_file(const TextFile &text_file,
                               const std::string &path,
                               const HtmlConfig &config) {
  auto output_path = path + "/text.html";
  std::ofstream ostream(output_path);
  if (!ostream.is_open()) {
    throw FileWriteError();
  }
  auto in = text_file.stream();
  HtmlWriter out(ostream, config.format_html, config.html_indent);

  out.write_begin();

  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  out.write_header_viewport(
      "width=device-width,initial-scale=1.0,user-scalable=yes");
  out.write_header_style_begin();
  out.out() << "*{font-family:monospace;}";
  out.out() << "td{padding-left:10px;padding-right:10px;}";
  out.write_header_style_end();
  out.write_header_end();

  out.write_body_begin();
  out.write_element_begin(
      "table",
      HtmlElementOptions().set_attributes(HtmlAttributesVector{
          {"cellpadding", "0"}, {"border", "0"}, {"cellspacing", "0"}}));

  for (std::uint32_t line = 1;; ++line) {
    out.write_element_begin("tr");

    out.write_element_begin("td",
                            HtmlElementOptions().set_inline(true).set_style(
                                "text-align:right;user-select:none;"));
    out.out() << line;
    out.write_element_end("td");

    out.write_element_begin("td");

    std::ostringstream ss_out;
    util::stream::getline(*in, ss_out);
    out.out() << escape_text(ss_out.str());

    out.write_element_end("td");
    out.write_element_end("tr");

    if (in->eof()) {
      break;
    }
  }

  out.write_element_end("table");
  out.write_body_end();

  out.write_end();

  return {text_file.file_type(), config, {{"text", output_path}}};
}

} // namespace odr::internal
