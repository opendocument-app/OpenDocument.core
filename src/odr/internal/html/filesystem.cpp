#include <odr/internal/html/filesystem.hpp>

#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

namespace odr::internal::html {
namespace {

class HtmlServiceImpl : public HtmlService {
public:
  HtmlServiceImpl(Filesystem filesystem, HtmlConfig config)
      : HtmlService(std::move(config)), m_filesystem{std::move(filesystem)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "files", "files.html"));
  }

  void warmup() const final {}

  [[nodiscard]] const HtmlViews &list_views() const final { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const final {
    if (path == "files.html") {
      return true;
    }

    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const final {
    if (path == "files.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const final {
    if (path == "files.html") {
      HtmlWriter writer(out, config());
      write_filesystem(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           html::HtmlWriter &out) const final {
    if (path == "files.html") {
      return write_filesystem(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_filesystem(HtmlWriter &out) const {
    HtmlResources resources;

    auto file_walker = m_filesystem.file_walker("");

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_meta("color-scheme", "light dark");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_style_begin();
    out.write_raw("*{font-family:monospace;}");
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();

    for (; !file_walker.end(); file_walker.next()) {
      common::Path file_path = common::Path(file_walker.path());
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

        File file = m_filesystem.open(file_path);

        out.write_element_begin("span");
        out.write_raw(std::to_string(file.size()));
        out.write_element_end("span");

        std::unique_ptr<std::istream> stream = file.stream();

        if (stream != nullptr) {
          out.write_element_begin("span");
          out.write_raw(" ");
          out.write_element_end("span");

          out.write_element_begin(
              "a",
              HtmlElementOptions().set_attributes(HtmlAttributesVector{
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

    return resources;
  }

protected:
  Filesystem m_filesystem;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

odr::HtmlService html::create_filesystem_service(const Filesystem &filesystem,
                                                 const std::string &cache_path,
                                                 HtmlConfig config) {
  (void)cache_path;

  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(filesystem, std::move(config)));
}

} // namespace odr::internal
