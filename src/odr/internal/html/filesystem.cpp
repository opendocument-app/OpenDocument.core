#include <odr/internal/html/filesystem.hpp>

#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <array>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace odr::internal::html {
namespace {

constexpr std::string_view listing_path = "files.html";

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

/// From the extension, not the bytes: sniffing every entry would read the whole
/// archive.
std::string mime_type_of(const Path &path) {
  const FileType type = file_type_by_file_extension(path.extension());
  if (type == FileType::unknown) {
    return "application/octet-stream";
  }
  return std::string(mimetype_by_file_type(type));
}

/// The whole entry inline, for a listing that has to stand alone.
std::optional<std::string> entry_data_url(const File &file,
                                          const std::string &mime_type) {
  const std::unique_ptr<std::istream> stream = file.stream();
  if (stream == nullptr) {
    return std::nullopt;
  }
  return file_to_url(*stream, mime_type);
}

/// Where an entry is written, relative to the listing. The archive names it, so
/// a path that escapes the output directory or collides with the listing gets
/// none, and stays inline instead.
std::optional<RelPath> entry_location(const Path &path) {
  const RelPath relative = path.make_relative();
  if (relative.escaping() || relative.empty() ||
      relative.string() == listing_path) {
    return std::nullopt;
  }
  return relative;
}

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(Filesystem filesystem, HtmlConfig config,
                  const Logger &logger)
      : HtmlService(std::move(config), logger),
        m_filesystem{std::move(filesystem)} {
    m_views.emplace_back(std::make_shared<HtmlView>(*this, "files", 0,
                                                    std::string(listing_path)));
  }

  /// Walking the archive is what turns up the entries, so the resources a host
  /// may ask for are only known once the listing has been written.
  void warmup() const override {
    std::lock_guard lock(m_mutex);

    if (m_warm) {
      return;
    }

    NullStream null;
    HtmlWriter out(null, config());
    m_resources = write_filesystem(out);

    m_warm = true;
  }

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (path == listing_path) {
      return true;
    }

    warmup();

    return resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == listing_path) {
      return "text/html";
    }
    warmup();

    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == listing_path) {
      HtmlWriter writer(out, config());
      write_filesystem(writer);
      return;
    }
    warmup();

    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      resource->write_resource(out);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == listing_path) {
      return write_filesystem(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  /// `nullopt` when the entry is to be written into the listing instead.
  HtmlResourceLocation locate_entry(const WritingState &state, const Path &path,
                                    const File &file) const {
    const std::optional<RelPath> entry_path = entry_location(path);
    if (!entry_path.has_value()) {
      return std::nullopt;
    }

    const odr::HtmlResource resource = HtmlResource::create(
        HtmlResourceType::file, mime_type_of(path), path.basename(),
        entry_path->string(), file, false, false, true);
    HtmlResourceLocation location =
        config().resource_locator(resource, config());
    // Two resources at one location write over each other; the stylesheet is
    // registered first, so an entry landing on it yields.
    if (location.has_value() &&
        resource_at(state.resources(), *location) != nullptr) {
      return std::nullopt;
    }

    state.resources().emplace_back(resource, location);
    return location;
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
    write_search_style(state);
    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("table",
                            HtmlElementOptions().set_class("odr-files"));

    // No header row: labels would be the only words in the page to translate.
    out.write_element_begin("tbody");

    // No directory rows: every entry names its whole path, and a directory is
    // the one row with nothing to open, save or measure.
    for (; !file_walker.end(); file_walker.next()) {
      if (!file_walker.is_file()) {
        continue;
      }

      const Path file_path(file_walker.path());
      const File file = m_filesystem.open(file_path.string());
      const std::string name = file_path.basename();
      const HtmlResourceLocation location =
          locate_entry(state, file_path, file);

      out.write_element_begin("tr");

      // An embedded entry is a `data:` URL, which no browser navigates to at
      // the top level, so there the path is text rather than a link.
      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-name"));
      if (location.has_value()) {
        out.write_element_begin(
            "a", HtmlElementOptions().set_inline(true).set_attributes(
                     HtmlAttributesVector{{"href", escape_attribute(*location)},
                                          {"title", escape_attribute(name)}}));
        out.write_raw(escape_text(file_path.string()));
        out.write_element_end("a");
      } else {
        out.write_raw(escape_text(file_path.string()));
      }
      out.write_element_end("td");

      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-size"));
      out.write_raw(human_size(file.size()));
      out.write_element_end("td");

      // The glyph has no name of its own; the file's is what a tooltip and a
      // screen reader read.
      out.write_element_begin(
          "td",
          HtmlElementOptions().set_inline(true).set_class("odr-files-action"));
      if (const std::optional<std::string> href =
              location.has_value()
                  ? std::optional(escape_attribute(*location))
                  : entry_data_url(file, mime_type_of(file_path));
          href.has_value()) {
        out.write_element_begin(
            "a", HtmlElementOptions().set_inline(true).set_attributes(
                     HtmlAttributesVector{{"href", *href},
                                          {"download", escape_attribute(name)},
                                          {"title", escape_attribute(name)}}));
        out.write_raw("\u2193");
        out.write_element_end("a");
      }
      out.write_element_end("td");

      out.write_element_end("tr");
    }

    out.write_element_end("tbody");
    out.write_element_end("table");

    write_search_script(state);

    out.write_body_end();

    out.write_end();

    return resources;
  }

protected:
  Filesystem m_filesystem;

  HtmlViews m_views;

  mutable std::mutex m_mutex;
  mutable bool m_warm = false;
  mutable HtmlResources m_resources;
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
