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

#include <sstream>

namespace odr::internal::html {
namespace {

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
    const WritingState state(out, config(), resources);

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

    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("div", HtmlElementOptions().set_class("odr-text"));

    // `aria-hidden`: the numbers are ours, not the file's - nothing reading the
    // page as content takes them.
    out.write_element_begin("div", HtmlElementOptions()
                                       .set_class("odr-text-nr")
                                       .set_extra(R"(aria-hidden="true")"));
    std::istringstream in(text);
    for (std::uint32_t line = 1; !in.eof(); ++line) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));
      out.out() << line;
      out.write_element_end("div");

      NullStream ss_out;
      util::stream::pipe_line(in, ss_out, false);
    }
    out.write_element_end("div");

    out.write_element_begin("div",
                            HtmlElementOptions().set_attributes(
                                [&](const HtmlAttributeWriterCallback &clb) {
                                  clb("class", "odr-text-body odr-text-wrap");
                                  if (config().editable) {
                                    clb("contenteditable", "true");
                                  }
                                }));
    in = std::istringstream(text);
    while (!in.eof()) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));

      std::ostringstream ss_out;
      util::stream::pipe_line(in, ss_out, false);
      if (std::string line = ss_out.str(); line.empty()) {
        out.write_element_begin(
            "br", HtmlElementOptions().set_close_type(HtmlCloseType::trailing));
      } else {
        out.out() << escape_text(std::move(line));
      }

      out.write_element_end("div");
    }
    out.write_element_end("div");

    out.write_element_end("div");

    write_search_script(state);
    write_text_script(state);
    write_viewport_script(state);

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
