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

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(Filesystem filesystem, HtmlConfig config,
                  const Logger &logger)
      : HtmlService(std::move(config), logger),
        m_filesystem{std::move(filesystem)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "files", 0, "files.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == "files.html";
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "files.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "files.html") {
      HtmlWriter writer(out, config());
      write_filesystem(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "files.html") {
      return write_filesystem(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_filesystem(HtmlWriter &out) const {
    HtmlResources resources;

    const FileWalker file_walker = m_filesystem.file_walker("/");

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);
    out.write_header_style_begin();
    out.write_raw("*{font-family:monospace;}");
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();

    const auto span = [&out](const HtmlWritable &content) {
      out.write_element_begin("span");
      out.write_raw(content);
      out.write_element_end("span");
    };

    for (; !file_walker.end(); file_walker.next()) {
      const Path file_path(file_walker.path());
      const bool is_file = file_walker.is_file();

      out.write_element_begin("p");

      span(escape_text(file_path.string()));
      span(" ");
      span(is_file ? "file" : "directory");

      if (is_file) {
        span(" ");

        File file = m_filesystem.open(file_path.string());

        span(std::to_string(file.size()));

        if (const std::unique_ptr<std::istream> stream = file.stream();
            stream != nullptr) {
          span(" ");

          out.write_element_begin(
              "a",
              HtmlElementOptions().set_attributes(HtmlAttributesVector{
                  {"href", file_to_url(*stream, "application/octet-stream")},
                  {"download", escape_attribute(file_path.basename())}}));
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

HtmlService html::create_filesystem_service(const Filesystem &filesystem,
                                            const std::string & /*cache_path*/,
                                            HtmlConfig config,
                                            const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(filesystem, std::move(config), logger));
}

} // namespace odr::internal
