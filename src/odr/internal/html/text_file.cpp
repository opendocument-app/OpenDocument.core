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
#include <sstream>
#include <string>

namespace odr::internal::html {
namespace {

/// 256 lines of 20px: a chunk the browser skips is a few screens tall.
constexpr std::uint32_t chunk_lines = 256;

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
    return path == "text.html" || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "text.html") {
      return "text/html";
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

  /// The bytes to write and the charset to declare. What we can decode becomes
  /// UTF-8; what we can only name passes through for the browser, which works
  /// because those encodings are ASCII-compatible and the page is ASCII.
  [[nodiscard]] std::pair<std::string, std::string> body_and_charset() const {
    const TextEncoding encoding = m_text_file.encoding();
    std::string text = m_text_file.text();

    if (encoding == TextEncoding::unknown ||
        text_encoding_is_decodable(encoding)) {
      return {std::move(text), "UTF-8"};
    }
    return {std::move(text), std::string(text_encoding_to_string(encoding))};
  }

  HtmlResources write_text(HtmlWriter &out) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources, logger());

    const auto [text, charset] = body_and_charset();

    out.write_begin();

    out.write_header_begin();

    out.write_header_charset(charset);
    out.write_header_title("odr");
    write_viewport_meta(out, config(), false);
    write_zoom_style(out, config(), WidthFit::none, {});
    write_content_margin_style(out, config());

    write_text_style(state);
    write_text_dark_style(state);
    write_search_style(state);
    write_search_dark_style(state);
    // without the script that marks the chunks near the viewport, the
    // browser decides that itself, at the cost of one layout of each
    out.write_raw("<noscript><style>.odr-chunk{content-visibility:auto}</"
                  "style></noscript>");

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

    std::istringstream in(text);
    std::uint32_t line_count = 0;
    for (; !in.eof(); ++line_count) {
      NullStream null;
      util::stream::pipe_line(in, null, false);
    }

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
    in = std::istringstream(text);
    for (std::uint32_t line = 0; line < line_count; ++line) {
      // the browser lays out only the chunks near the viewport, which
      // `text.js` marks
      const bool first = line % chunk_lines == 0;
      if (first) {
        if (line > 0) {
          out.write_element_end("div");
        }
        // the first chunk is near the viewport before the script says so
        HtmlElementOptions chunk;
        chunk.set_class(line == 0 ? "odr-chunk odr-chunk-near" : "odr-chunk");
        if (const std::uint32_t lines = line_count - line;
            lines < chunk_lines) {
          chunk.set_style("--odr-text-lines:" + std::to_string(lines));
        }
        out.write_element_begin("div", chunk);
      }

      // a line number is a counter, which the containment of a chunk hides
      // from a counter on the chunk itself, so its first line states where
      // it counts from
      HtmlElementOptions line_options;
      line_options.set_inline(true);
      if (first) {
        line_options.set_style("counter-reset:odr-line " +
                               std::to_string(line));
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
    out.write_element_end("div"); // the last chunk
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

protected:
  TextFile m_text_file;
  /// The css and js this view links; empty of locations when the config embeds
  /// them.
  HtmlResources m_resources;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_text_service(const TextFile &text_file,
                                      HtmlConfig config, const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(text_file, std::move(config), logger));
}

} // namespace odr::internal
