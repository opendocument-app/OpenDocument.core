#include <fstream>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/text_file.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <sstream>

namespace odr::internal {

Html html::translate_text_file(const TextFile &text_file,
                               const std::string &path,
                               const HtmlConfig &config) {
  auto output_path = path + "/text.html";
  std::ofstream out(output_path);
  if (!out.is_open()) {
    throw FileWriteError();
  }

  out << internal::html::doctype();
  out << "<html><head>";
  out << internal::html::default_headers();
  out << "<style>";
  // TODO style
  out << "*{font-family:monospace;}";
  out << "td{padding-left:10px;padding-right:10px;}";
  out << "</style>";
  out << "</head>";

  out << "<body " << internal::html::body_attributes(config) << ">";

  {
    auto in = text_file.stream();
    std::uint32_t line = 1;

    out << "<table";
    out << R"( cellpadding="0" border="0" cellspacing="0")";
    out << ">";

    while (true) {
      out << "<tr><td style=\"text-align:right;user-select:none;\">" << line
          << "</td>";
      out << "<td>";

      std::ostringstream ss_out;
      util::stream::getline(*in, ss_out);
      out << escape_text(ss_out.str());

      out << "</td></tr>";
      if (in->eof()) {
        break;
      }
      ++line;
    }
  }

  out << "</body>";
  out << "</html>";

  return {text_file.file_type(), config, {{"text", output_path}}};
}

} // namespace odr::internal
