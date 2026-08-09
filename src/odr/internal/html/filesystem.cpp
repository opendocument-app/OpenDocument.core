#include <odr/internal/html/filesystem.hpp>

#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <array>
#include <iomanip>
#include <sstream>

namespace odr::internal::html {
namespace {

/// Binary multiples, named as such — nothing to guess the base of.
std::string human_size(const std::size_t size) {
  static constexpr std::array<const char *, 5> units{"B", "KiB", "MiB", "GiB",
                                                     "TiB"};

  double value = static_cast<double>(size);
  std::size_t unit = 0;
  while (value >= 1024.0 && unit + 1 < units.size()) {
    value /= 1024.0;
    ++unit;
  }

  std::ostringstream result;
  result << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << " "
         << units.at(unit);
  return result.str();
}

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
    const WritingState state(out, config(), resources);

    const FileWalker file_walker = m_filesystem.file_walker("/");

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);
    write_filesystem_style(state);
    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("table",
                            HtmlElementOptions().set_class("odr-files"));

    // No header row: labels would be the only words in the page to translate.
    out.write_element_begin("tbody");

    for (; !file_walker.end(); file_walker.next()) {
      const Path file_path(file_walker.path());
      const bool is_file = file_walker.is_file();

      out.write_element_begin("tr", HtmlElementOptions().set_class(
                                        is_file ? std::nullopt
                                                : std::optional<HtmlWritable>(
                                                      "odr-files-directory")));

      // A trailing separator says "directory" without a column saying so.
      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-name"));
      out.write_raw(
          escape_text(is_file ? file_path.string() : file_path.string() + "/"));
      out.write_element_end("td");

      const File file =
          is_file ? m_filesystem.open(file_path.string()) : File();

      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-size"));
      if (is_file) {
        out.write_raw(human_size(file.size()));
      }
      out.write_element_end("td");

      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-action"));
      if (is_file) {
        if (const std::unique_ptr<std::istream> stream = file.stream();
            stream != nullptr) {
          const std::string name = file_path.basename();
          // The glyph has no name of its own; the file's is what a tooltip and
          // a screen reader read.
          out.write_element_begin(
              "a", HtmlElementOptions().set_inline(true).set_attributes(
                       HtmlAttributesVector{
                           {"href",
                            file_to_url(*stream, "application/octet-stream")},
                           {"download", escape_attribute(name)},
                           {"title", escape_attribute(name)}}));
          out.write_raw("\u2193");
          out.write_element_end("a");
        }
      }
      out.write_element_end("td");

      out.write_element_end("tr");
    }

    out.write_element_end("tbody");
    out.write_element_end("table");

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
                                            HtmlConfig config,
                                            const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(filesystem, std::move(config), logger));
}

} // namespace odr::internal
