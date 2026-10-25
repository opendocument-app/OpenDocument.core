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

#include <algorithm>
#include <charconv>
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

/// The chunks that one script of the search index holds the text of.
constexpr std::size_t chunks_per_index = 64;

/// Where the page loads the text of index group @p group from
/// (`lazy-loading.md` decision 8).
std::string index_path(const std::size_t group) {
  return "text-search/" + std::to_string(group) + ".js";
}

/// The number in @p path after @p prefix, as @p write writes it, nothing where
/// it holds none.
std::optional<std::size_t> number_of(const std::string_view path,
                                     const std::string_view prefix,
                                     std::string (*const write)(std::size_t)) {
  if (!path.starts_with(prefix)) {
    return std::nullopt;
  }
  const std::string_view digits = path.substr(prefix.size());
  std::size_t number = 0;
  // one path for each number, the one `write` writes: no leading zero
  if (std::from_chars(digits.data(), digits.data() + digits.size(), number)
              .ec != std::errc{} ||
      write(number) != path) {
    return std::nullopt;
  }
  return number;
}

/// The chunk a path names, nothing where it names none.
std::optional<std::size_t> chunk_of(const std::string_view path) {
  return number_of(path, "text-chunks/", chunk_path);
}

/// The index group a path names, nothing where it names none.
std::optional<std::size_t> index_of(const std::string_view path) {
  return number_of(path, "text-search/", index_path);
}

/// The next line of @p in, without its end.
std::string read_line(std::istream &in) {
  std::ostringstream line;
  util::stream::pipe_line(in, line, false);
  return std::move(line).str();
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

    if (std::string text_line = read_line(in); text_line.empty()) {
      out.write_element_begin(
          "br", HtmlElementOptions().set_close_type(HtmlCloseType::trailing));
    } else {
      out.out() << escape_text(std::move(text_line));
    }

    out.write_element_end("div");
  }
}

class HtmlServiceImpl;

/// A script the page loads, a chunk or a part of the search index, which
/// `bring_offline` writes beside the page, and the HTTP server serves, from the
/// service on demand.
class ScriptResource final : public abstract::HtmlResource {
public:
  ScriptResource(const HtmlServiceImpl &service, std::string path)
      : m_service{&service}, m_path{std::move(path)} {}

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
    if (const std::optional<std::size_t> group = index_of(path);
        group.has_value()) {
      // the first chunk is in the page, so no group holds only that one
      return loads_chunks() &&
             std::max<std::size_t>(*group * chunks_per_index, 1) <
                 lines().offsets.size();
    }
    return path == "text.html" || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "text.html") {
      return "text/html";
    }
    if ((chunk_of(path).has_value() || index_of(path).has_value()) &&
        exists(path)) {
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
    if (const std::optional<std::size_t> chunk = chunk_of(path);
        chunk.has_value() && exists(path)) {
      write_chunk(*chunk, out);
      return;
    }
    if (const std::optional<std::size_t> group = index_of(path);
        group.has_value() && exists(path)) {
      write_index(*group, out);
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

    const bool loads = loads_chunks();
    const Lines read = loads ? Lines{} : read_lines();
    const Lines &all = loads ? lines() : read;

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
        HtmlAttributesVector attributes{
            {"data-odr-chunk", std::to_string(index)},
            {"data-odr-chunk-src", chunk_path(index)}};
        resources.emplace_back(
            odr::HtmlResource(
                std::make_shared<ScriptResource>(*this, chunk_path(index))),
            chunk_path(index));
        // the first chunk of a group that loads names the script of its text,
        // which holds the text of the group
        if (index == 1 || index % chunks_per_index == 0) {
          const std::string text = index_path(index / chunks_per_index);
          attributes.emplace_back("data-odr-chunk-text", text);
          resources.emplace_back(
              odr::HtmlResource(std::make_shared<ScriptResource>(*this, text)),
              text);
        }
        chunk.set_attributes(std::move(attributes));
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

  /// The text of index group @p group, as the script that hands it to the
  /// page: the lines of each chunk that loads, with a `\n` after each line
  /// but the last, because a line is a block that a search does not match
  /// across.
  void write_index(const std::size_t group, std::ostream &out) const {
    const Lines &all = lines();
    std::istringstream in(all.text);
    const std::size_t first =
        std::max<std::size_t>(group * chunks_per_index, 1);
    const std::size_t end =
        std::min(all.offsets.size(), (group + 1) * chunks_per_index);
    in.seekg(static_cast<std::streamoff>(all.offsets.at(first)));
    out << "odr.chunks.text({";
    for (std::size_t index = first; index < end; ++index) {
      const std::size_t line_end =
          std::min(all.count, (index + 1) * chunk_lines);
      std::string text;
      for (std::size_t line = index * chunk_lines; line < line_end; ++line) {
        if (line > index * chunk_lines) {
          text += '\n';
        }
        text += read_line(in);
      }
      out << (index > first ? "," : "") << script_string(std::to_string(index))
          << ":" << script_string(text);
    }
    out << "});\n";
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

  [[nodiscard]] Lines read_lines() const {
    Lines result;
    result.text = m_text_file.text();
    std::istringstream in(result.text);
    while (!in.eof()) {
      if (result.count % chunk_lines == 0) {
        result.offsets.push_back(static_cast<std::size_t>(in.tellg()));
      }
      NullStream null;
      util::stream::pipe_line(in, null, false);
      ++result.count;
    }
    return result;
  }

  /// Read once: a chunk the page asks for is found without reading the text
  /// up to it again. Only a page that loads its chunks reads it, because an
  /// edit of the file changes the text under the service.
  [[nodiscard]] const Lines &lines() const {
    std::call_once(m_lines_once, [this] { m_lines = read_lines(); });
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

void ScriptResource::write_resource(std::ostream &os) const {
  m_service->write(m_path, os);
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
