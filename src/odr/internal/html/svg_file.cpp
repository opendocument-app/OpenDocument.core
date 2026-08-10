#include <odr/internal/html/svg_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/frontend.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/svg/svg_file.hpp>
#include <odr/internal/svg/svg_util.hpp>
#include <odr/internal/xml/xml_file.hpp>

#include <pugixml.hpp>

#include <memory>
#include <ostream>
#include <string>
#include <utility>

namespace odr::internal::html {
namespace {

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(std::shared_ptr<svg::SvgFile> svg_file, HtmlConfig config,
                  const Logger &logger)
      : HtmlService(std::move(config), logger), m_svg_file{std::move(svg_file)},
        m_resources{locate_svg_resources(this->config())} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "image", 0, "image.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == "image.html" || resource_at(m_resources, path) != nullptr;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "image.html") {
      return "text/html";
    }
    if (const odr::HtmlResource *resource = resource_at(m_resources, path);
        resource != nullptr) {
      return resource->mime_type();
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "image.html") {
      HtmlWriter writer(out, config());
      write_svg(writer);
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
    if (path == "image.html") {
      return write_svg(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_svg(HtmlWriter &out) const {
    HtmlResources resources;
    const WritingState state(out, config(), resources);

    const std::unique_ptr<pugi::xml_document> document =
        xml::parse_source(m_svg_file->text());
    svg::drop_namespace_prefixes(document->document_element());
    svg::sanitize(document->document_element());

    out.write_begin();

    out.write_header_begin();

    out.write_header_charset("UTF-8");
    // second line of defence after the scrub above, and free: no view of an
    // svg has a script of its own
    out.write_header_meta_equiv("Content-Security-Policy", "script-src 'none'");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    write_viewport_meta(out, config(), true);

    write_svg_style(state);

    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("div", HtmlElementOptions().set_class("odr-svg"));
    document->document_element().print(out.out(), "", pugi::format_raw);
    out.write_element_end("div");

    out.write_body_end();

    out.write_end();

    return resources;
  }

protected:
  std::shared_ptr<svg::SvgFile> m_svg_file;
  /// The css this view links; empty of locations when the config embeds it.
  HtmlResources m_resources;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService
html::create_svg_service(const std::shared_ptr<svg::SvgFile> &svg_file,
                         HtmlConfig config, const Logger &logger) {
  return odr::HtmlService(
      std::make_unique<HtmlServiceImpl>(svg_file, std::move(config), logger));
}

} // namespace odr::internal
