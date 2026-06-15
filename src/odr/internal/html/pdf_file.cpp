#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_page_text.hpp>

#include <cmath>

namespace odr::internal::html {

namespace {

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(PdfFile pdf_file, HtmlConfig config,
                  std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_pdf_file{std::move(pdf_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "document", 0, "document.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (path == "document.html") {
      return true;
    }

    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "document.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "document.html") {
      HtmlWriter writer(out, config());
      write_document(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "document.html") {
      return write_document(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_document(HtmlWriter &out) const {
    HtmlResources resources;

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    pdf::DocumentParser parser = pdf_file.create_parser(*m_logger);
    const std::unique_ptr<pdf::Document> document = parser.parse_document();

    const std::vector<pdf::Page *> pages = document->collect_pages();

    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    // Constant per-page and per-glyph styling lives in classes so it is not
    // repeated inline on every one of the (potentially millions of) spans.
    out.write_header_style_begin();
    out.out() << ".p{position:relative}";
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre}";
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();

    // CSS uses 96px to the inch, PDF user space 72 units to the inch.
    static constexpr double pt_to_px = 96.0 / 72.0;
    static constexpr double pt_to_in = 1 / 72.0;

    for (pdf::Page *page : pages) {
      const pdf::Array &page_box = page->media_box.as_array();
      const double box_x0 = page_box[0].as_real();
      const double box_y0 = page_box[1].as_real();
      const double width = page_box[2].as_real() - box_x0;
      const double height = page_box[3].as_real() - box_y0;

      out.write_element_begin(
          "div",
          HtmlElementOptions().set_class("p").set_style([&](std::ostream &o) {
            o << "width:" << width * pt_to_in << "in;";
            o << "height:" << height * pt_to_in << "in;";
          }));

      std::string stream;
      for (const auto &content_reference : page->contents_reference) {
        // streams of one page join at token boundaries (ISO 32000-1 7.7.3.3)
        stream += parser.read_decoded_stream(content_reference);
        stream += '\n';
      }

      // Map PDF user space (origin at the MediaBox corner, y up) to the page
      // box in CSS pixels (origin top-left, y down). `flip_glyph` un-mirrors
      // the glyphs so text stays upright after the page flip.
      constexpr util::math::Transform2D flip_glyph =
          util::math::Transform2D::scaling(1, -1);
      const util::math::Transform2D to_box =
          util::math::Transform2D::translation(-box_x0, -box_y0) *
          util::math::Transform2D::translation_scaling(1, -1, 0, height);

      // Round CSS coordinates to 0.01px; sub-pixel precision beyond that is
      // invisible and the extra digits add up over millions of spans.
      const auto round2 = [](const double v) {
        return std::round(v * 100.0) / 100.0;
      };

      for (const pdf::TextElement &text :
           pdf::extract_text(stream, *page->resources, *m_logger)) {
        const util::math::Transform2D m = flip_glyph * text.transform * to_box;

        out.write_element_begin(
            "span",
            HtmlElementOptions().set_class("t").set_style([&](std::ostream &o) {
              // TODO baseline sits at the box top until font ascent
              // metrics land
              if (m.b == 0 && m.c == 0 && m.a == m.d) {
                // Upright uniform scale: fold the scale into the
                // font size and place the origin with left/top, so
                // the (otherwise near-universal) matrix is dropped.
                o << "left:" << round2(m.e * pt_to_px) << "px;";
                o << "top:" << round2(m.f * pt_to_px) << "px;";
                o << "font-size:" << round2(m.a * text.size * pt_to_px)
                  << "px;";
              } else {
                o << "transform:matrix(" << m.a << "," << m.b << "," << m.c
                  << "," << m.d << "," << round2(m.e * pt_to_px) << ","
                  << round2(m.f * pt_to_px) << ");";
                o << "font-size:" << round2(text.size * pt_to_px) << "px;";
              }
            }));
        out.write_raw(escape_text(text.text));
        out.write_element_end("span");
      }

      out.write_element_end("div");
    }

    out.write_body_end();
    out.write_end();

    return resources;
  }

protected:
  PdfFile m_pdf_file;

  HtmlViews m_views;
};

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

HtmlService html::create_pdf_service(const PdfFile &pdf_file,
                                     const std::string & /*cache_path*/,
                                     HtmlConfig config,
                                     std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      pdf_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal
