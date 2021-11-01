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
  out << "</style>";
  out << "</head>";

  out << "<body " << internal::html::body_attributes(config) << ">";

  util::stream::pipe(*text_file.stream(), out);

  out << "</body>";
  out << "</html>";

  return {text_file.file_type(), config, {{"document", output_path}}};
}

} // namespace odr::internal
