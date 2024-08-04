#include <odr/internal/html/filesystem.hpp>

#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <fstream>

namespace odr::internal {

Html html::translate_filesystem(FileType file_type,
                                const Filesystem &filesystem,
                                const std::string &output_path,
                                const HtmlConfig &config) {
  std::string output_file_path = output_path + "/files.html";
  std::ofstream ostream(output_file_path);
  if (!ostream.is_open()) {
    throw FileWriteError();
  }
  HtmlWriter out(ostream, config.format_html, config.html_indent);

  auto file_walker = filesystem.file_walker("");

  out.write_begin();

  out.write_header_begin();
  out.write_header_charset("UTF-8");
  out.write_header_target("_blank");
  out.write_header_title("odr");
  out.write_header_viewport(
      "width=device-width,initial-scale=1.0,user-scalable=yes");
  out.write_header_style_begin();
  out.write_raw("*{font-family:monospace;}");
  out.write_header_style_end();
  out.write_header_end();

  out.write_body_begin();

  for (; !file_walker.end(); file_walker.next()) {
    common::Path file_path = file_walker.path();
    bool is_file = file_walker.is_file();

    out.write_element_begin("p");

    out.write_element_begin("span");
    out.write_raw(file_path.string());
    out.write_element_end("span");

    out.write_element_begin("span");
    out.write_raw(" ");
    out.write_element_end("span");

    out.write_element_begin("span");
    out.write_raw(file_walker.is_file() ? "file" : "directory");
    out.write_element_end("span");

    if (is_file) {
      out.write_element_begin("span");
      out.write_raw(" ");
      out.write_element_end("span");

      File file = filesystem.open(file_path);

      out.write_element_begin("span");
      out.write_raw(std::to_string(file.size()));
      out.write_element_end("span");

      std::unique_ptr<std::istream> stream = file.stream();

      if (stream != nullptr) {
        out.write_element_begin("span");
        out.write_raw(" ");
        out.write_element_end("span");

        out.write_element_begin(
            "a", HtmlElementOptions().set_attributes(HtmlAttributesVector{
                     {"href", file_to_url(*stream, "application/octet-stream")},
                     {"download", file_path.basename()}}));
        out.write_raw("download");
        out.write_element_end("a");
      }
    }

    out.write_element_end("p");
  }

  out.write_body_end();

  out.write_end();

  return {file_type, config, {{"files", output_file_path}}};
}

} // namespace odr::internal
