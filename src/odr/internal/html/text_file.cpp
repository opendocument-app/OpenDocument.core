#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
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

    const std::unique_ptr<std::istream> in = m_text_file.stream();

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_style_begin();
    out.write_raw("*{font-family:monospace;}");
    out.write_raw("td{padding-left:3pt;padding-right:3pt}");
    out.write_raw("td:first-child{text-align:right;vertical-align:top;user-"
                  "select:none;color:#999999;border-right:solid #999999;}");
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();
    out.write_element_begin("table",
                            HtmlElementOptions().set_attributes(
                                [&](const HtmlAttributeWriterCallback &clb) {
                                  clb("cellpadding", "0");
                                  clb("border", "0");
                                  clb("cellspacing", "0");
                                  if (config().editable) {
                                    clb("contenteditable", "plaintext-only");
                                  }
                                }));

    for (std::uint32_t line = 1; !in->eof(); ++line) {
      out.write_element_begin("tr");

      out.write_element_begin(
          "td", HtmlElementOptions().set_inline(true).set_attributes(
                    [&](const HtmlAttributeWriterCallback &clb) {
                      if (config().editable) {
                        clb("contenteditable", "false");
                      }
                    }));
      out.out() << line;
      out.write_element_end("td");

      out.write_element_begin("td", HtmlElementOptions().set_inline(true));

      std::ostringstream ss_out;
      util::stream::pipe_line(*in, ss_out, false);
      out.out() << escape_text(ss_out.str());

      out.write_element_end("td");

      out.write_element_end("tr");
    }

    out.write_element_end("table");
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

odr::HtmlService
html::create_text_service(const TextFile &text_file,
                          [[maybe_unused]] const std::string &cache_path,
                          HtmlConfig config, std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      text_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal
