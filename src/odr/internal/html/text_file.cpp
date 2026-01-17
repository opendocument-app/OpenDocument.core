#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <sstream>

namespace odr::internal::html {
namespace {

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(TextFile text_file, HtmlConfig config,
                  std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_text_file{std::move(text_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "text", 0, "text.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (path == "text.html") {
      return true;
    }

    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "text.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "text.html") {
      HtmlWriter writer(out, config());
      write_text(writer);
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

  HtmlResources write_text(HtmlWriter &out) const {
    HtmlResources resources;

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_style_begin();
    out.write_raw(
        ".odr-text{display:flex;flex-direction:row;font-family:monospace;}");
    out.write_raw(
        ".odr-text-nr{display:flex;flex-direction:column;text-align:"
        "right;vertical-align:top;color:#999999;border-right:solid #999999;}");
    out.write_raw(".odr-text-body{display:flex;flex-direction:column;padding-"
                  "left:5pt;white-space:pre;}");
    out.write_raw(".odr-text-wrap{white-space:break-spaces;word-break:break-"
                  "word;overflow-wrap:anywhere;flex-shrink:1;}");
    out.write_raw("[contenteditable]:focus{outline:none;}");
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("div", HtmlElementOptions().set_class("odr-text"));

    out.write_element_begin("div",
                            HtmlElementOptions().set_class("odr-text-nr"));
    std::unique_ptr<std::istream> in = m_text_file.stream();
    for (std::uint32_t line = 1; !in->eof(); ++line) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));
      out.out() << line;
      out.write_element_end("div");

      NullStream ss_out;
      util::stream::pipe_line(*in, ss_out, false);
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
    in = m_text_file.stream();
    while (!in->eof()) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));

      std::ostringstream ss_out;
      util::stream::pipe_line(*in, ss_out, false);
      const std::string &line = ss_out.str();
      if (line.empty()) {
        out.write_element_begin(
            "br", HtmlElementOptions().set_close_type(HtmlCloseType::trailing));
      } else {
        out.out() << ss_out.str();
      }

      out.write_element_end("div");
    }
    out.write_element_end("div");

    out.write_element_end("div");

    out.write_element_begin("script");
    out.out() << R"(
function updateLineNumberHeight() {
    const nrCells = document.querySelectorAll('.odr-text-nr > div');
    const textCells = document.querySelectorAll('.odr-text-body > div');

    for (let i = 0; i < textCells.length; i++) {
            const height = textCells[i].offsetHeight;
            nrCells[i].style.height = height + 'px';
    }
}

const textBody = document.querySelector('.odr-text-body');

const resizeObserver = new ResizeObserver(entries => {
    updateLineNumberHeight();
});
resizeObserver.observe(textBody);

textBody.addEventListener('input', (event) => {
    const nrCells = document.querySelectorAll('.odr-text-nr > div');
    const textCells = document.querySelectorAll('.odr-text-body > div');

    const nrCount = nrCells.length;
    const lineCount = textCells.length;
    if (lineCount > nrCount) {
        for (let i = nrCount + 1; i <= lineCount; i++) {
            const nrCell = document.createElement('div');
            nrCell.textContent = i;
            document.querySelector('.odr-text-nr').appendChild(nrCell);
        }
    } else if (lineCount < nrCount) {
        for (let i = nrCount; i > lineCount; --i) {
            document.querySelector('.odr-text-nr').removeChild(
                document.querySelector('.odr-text-nr').lastChild);
        }
    }

    updateLineNumberHeight();
});
)";
    out.write_element_end("script");

    out.write_body_end();

    out.write_end();

    return resources;
  }

protected:
  TextFile m_text_file;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService
html::create_text_service(const TextFile &text_file,
                          [[maybe_unused]] const std::string &cache_path,
                          HtmlConfig config, std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      text_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal
