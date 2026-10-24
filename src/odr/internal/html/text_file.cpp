#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace odr::internal::html {
namespace {

/// 256 lines of 20px: a chunk the browser skips is a few screens tall.
constexpr std::uint32_t chunk_lines = 256;

/// Where the page loads chunk @p index from (`lazy-loading.md` decision 3).
std::string chunk_path(const std::size_t index) {
  return "text-chunks/" + std::to_string(index) + ".js";
}

/// The chunk a path names, nothing where it names none.
std::optional<std::size_t> chunk_of(const std::string_view path) {
  constexpr std::string_view prefix = "text-chunks/";
  constexpr std::string_view suffix = ".js";
  if (!path.starts_with(prefix) || !path.ends_with(suffix) ||
      path.size() <= prefix.size() + suffix.size()) {
    return std::nullopt;
  }
  std::size_t index = 0;
  for (const char c : path.substr(prefix.size(), path.size() - prefix.size() -
                                                     suffix.size())) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    index = index * 10 + static_cast<std::size_t>(c - '0');
  }
  return index;
}

/// The lines from @p in, @p count of them at most, the first of which is line
/// @p first. A chunk has style containment, so the line counter does not count
/// on past it, and its first line sets where it starts.
void write_lines(HtmlWriter &out, std::istream &in, const std::uint32_t first,
                 const std::uint32_t count) {
  for (std::uint32_t line = first; line < first + count && !in.eof(); ++line) {
    HtmlElementOptions line_options;
    line_options.set_inline(true);
    if (line == first) {
      line_options.set_style("counter-reset:odr-line " + std::to_string(line));
    }
    out.write_element_begin("div", line_options);

    std::ostringstream ss_out;
    util::stream::pipe_line(in, ss_out, false);
    if (std::string text_line = ss_out.str(); text_line.empty()) {
      out.write_element_begin(
          "br", HtmlElementOptions().set_close_type(HtmlCloseType::trailing));
    } else {
      out.out() << escape_text(std::move(text_line));
    }

    out.write_element_end("div");
  }
}

class HtmlServiceImpl;

/// A chunk the page loads as a script, which `bring_offline` writes beside
/// the page, and the HTTP server serves, from the service on demand.
class ChunkResource final : public abstract::HtmlResource {
public:
  ChunkResource(const HtmlServiceImpl &service, const std::size_t index)
      : m_service{&service}, m_index{index}, m_path{chunk_path(index)} {}

  [[nodiscard]] HtmlResourceType type() const override {
    return HtmlResourceType::js;
  }
  [[nodiscard]] const std::string &mime_type() const override {
    static const std::string mime_type = "text/javascript";
    return mime_type;
  }
  [[nodiscard]] const std::string &name() const override { return m_path; }
  [[nodiscard]] const std::string &path() const override { return m_path; }
  [[nodiscard]] const std::optional<File> &file() const override {
    static const std::optional<File> none;
    return none;
  }
  [[nodiscard]] bool is_shipped() const override { return false; }
  [[nodiscard]] bool is_external() const override { return false; }
  [[nodiscard]] bool is_accessible() const override { return true; }

  void write_resource(std::ostream &os) const override;

private:
  const HtmlServiceImpl *m_service{nullptr};
  std::size_t m_index{0};
  std::string m_path;
};

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(TextFile text_file, HtmlConfig config, const Logger &logger)
      : HtmlService(std::move(config), logger),
        m_text_file{std::move(text_file)},
        m_resources{locate_text_resources(this->config())} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "text", 0, "text.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (const std::optional<std::size_t> chunk = chunk_of(path);
        chunk.has_value()) {
      return loads_chunks() && *chunk > 0 && *chunk < lines().offsets.size();
    }
    return path == "text.html" || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "text.html") {
      return "text/html";
    }
    if (exists(path) && chunk_of(path).has_value()) {
      return "text/javascript";
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "text.html") {
      HtmlWriter writer(out, config());
      write_text(writer);
      return;
    }
    if (exists(path) && chunk_of(path).has_value()) {
      write_chunk(*chunk_of(path), out);
      return;
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      resource->write_resource(out);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "text.html") {
      return write_text(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  /// The charset to declare. What we can decode becomes UTF-8; what we can
  /// only name passes through for the browser, which works because those
  /// encodings are ASCII-compatible and the page is ASCII.
  [[nodiscard]] std::string charset() const {
    const TextEncoding encoding = m_text_file.encoding();
    if (encoding == TextEncoding::unknown ||
        text_encoding_is_decodable(encoding)) {
      return "UTF-8";
    }
    return std::string(text_encoding_to_string(encoding));
  }

  HtmlResources write_text(HtmlWriter &out) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources, logger());

    const Lines &all = lines();

    out.write_begin();

    out.write_header_begin();

    out.write_header_charset(charset());
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);
    write_zoom_style(out, config(), WidthFit::none, {});
    write_content_margin_style(out, config());

    write_text_style(state);
    write_text_dark_style(state);
    write_search_style(state);
    write_search_dark_style(state);
    write_chunk_style(state);

    out.write_header_end();

    out.write_body_begin(HtmlElementOptions().set_attributes(
        [&](const HtmlAttributeWriterCallback &clb) {
          // what `enable()` answers, stated only by a render that edits
          if (config().editable) {
            clb("data-odr-editable",
                m_text_file.is_savable() ? "true" : "readOnly");
          }
          clb("data-odr-keyboard", keyboard_classes(config()));
        }));

    const auto line_count = static_cast<std::uint32_t>(all.count);
    const bool loads = loads_chunks();

    out.write_element_begin(
        "div",
        HtmlElementOptions()
            .set_class("odr-text")
            .set_style("--odr-text-digits:" +
                       std::to_string(std::to_string(line_count).size())));

    // `contenteditable` is not written: `odr.editing.enable()` puts it on
    // these lines, so one render serves both modes.
    out.write_element_begin(
        "div", HtmlElementOptions().set_class("odr-text-body odr-text-wrap"));
    std::istringstream in(all.text);
    // the browser lays out only the chunks near the viewport; without that,
    // all the lines are one block
    const std::uint32_t lines_per_block =
        config().lazy_layout ? chunk_lines : line_count;
    for (std::uint32_t line = 0; line < line_count; line += lines_per_block) {
      const std::size_t index = line / lines_per_block;
      const std::uint32_t lines_in_block =
          std::min(lines_per_block, line_count - line);
      // the first chunk is near the viewport before the script says so, and
      // a later one loads its lines once it is near
      const bool placeholder = loads && index > 0;
      HtmlElementOptions chunk;
      chunk.set_class("odr-text-lines" + chunk_classes(config(), line == 0));
      if (config().lazy_layout && lines_in_block < chunk_lines) {
        chunk.set_style("--odr-text-lines:" + std::to_string(lines_in_block));
      }
      if (placeholder) {
        chunk.set_attributes(
            HtmlAttributesVector{{"data-odr-chunk", std::to_string(index)},
                                 {"data-odr-chunk-src", chunk_path(index)}});
        resources.emplace_back(
            odr::HtmlResource(std::make_shared<ChunkResource>(*this, index)),
            chunk_path(index));
      }
      out.write_element_begin("div", chunk);
      if (!placeholder) {
        in.seekg(static_cast<std::streamoff>(all.offsets[index]));
        write_lines(out, in, line, lines_in_block);
      }
      out.write_element_end("div");
    }
    out.write_element_end("div");
    out.write_element_end("div");

    write_search_script(state);
    write_editing_script(state);
    write_text_script(state);
    write_viewport_script(state);
    write_host_bridge_script(state);

    out.write_body_end();

    out.write_end();

    return resources;
  }

  /// Chunk @p index as the script that puts its lines into the page.
  void write_chunk(const std::size_t index, std::ostream &out) const {
    const Lines &all = lines();
    std::istringstream in(all.text);
    in.seekg(static_cast<std::streamoff>(all.offsets.at(index)));
    std::ostringstream html;
    HtmlWriter writer(html, false, "");
    const auto first = static_cast<std::uint32_t>(index * chunk_lines);
    write_lines(
        writer, in, first,
        std::min<std::uint32_t>(chunk_lines,
                                static_cast<std::uint32_t>(all.count) - first));
    out << "odr.chunks.fill(" << index << "," << script_string(html.str())
        << ");\n";
  }

  /// Whether the page loads its later chunks, which it does where they are
  /// laid out lazily, read only, and spelled in UTF-8, which a script is.
  [[nodiscard]] bool loads_chunks() const {
    return config().lazy_loading && config().lazy_layout &&
           !config().editable && charset() == "UTF-8";
  }

protected:
  /// The text, how many lines it has, and where each chunk starts in it.
  struct Lines final {
    std::string text;
    std::size_t count{0};
    std::vector<std::size_t> offsets;
  };

  /// Read once: a chunk the page asks for is found without reading the text
  /// up to it again.
  [[nodiscard]] const Lines &lines() const {
    std::call_once(m_lines_once, [this] {
      m_lines.text = m_text_file.text();
      std::istringstream in(m_lines.text);
      while (!in.eof()) {
        if (m_lines.count % chunk_lines == 0) {
          m_lines.offsets.push_back(static_cast<std::size_t>(in.tellg()));
        }
        NullStream null;
        util::stream::pipe_line(in, null, false);
        ++m_lines.count;
      }
    });
    return m_lines;
  }

  mutable std::once_flag m_lines_once;
  mutable Lines m_lines;

  TextFile m_text_file;
  /// The css and js this view links; empty of locations when the config embeds
  /// them.
  HtmlResources m_resources;

  HtmlViews m_views;
};

void ChunkResource::write_resource(std::ostream &os) const {
  m_service->write_chunk(m_index, os);
}

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_text_service(const TextFile &text_file,
                                      HtmlConfig config, const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(text_file, std::move(config), logger));
}

} // namespace odr::internal
