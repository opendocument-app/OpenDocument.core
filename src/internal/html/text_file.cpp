#include <fstream>
#include <internal/html/common.h>
#include <internal/html/text_file.h>
#include <internal/util/stream_util.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>

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
      util::stream::getline(*in, out);
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
