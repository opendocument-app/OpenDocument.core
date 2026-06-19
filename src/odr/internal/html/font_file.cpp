#include <odr/internal/html/font_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/otf_writer.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <algorithm>
#include <memory>
#include <string>

namespace odr::internal::html {
namespace {

constexpr std::uint16_t max_grid_glyphs = 4096;

std::string escape(const std::string &text) {
  std::string out;
  for (const char c : text) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    default:
      out += c;
    }
  }
  return out;
}

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(FontFile font_file, HtmlConfig config,
                  std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_font_file{std::move(font_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "font", 0, "font.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    return path == "font.html";
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "font.html") {
      return "text/html";
    }
    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "font.html") {
      HtmlWriter writer(out, config());
      write_font(writer);
      return;
    }
    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "font.html") {
      return write_font(out);
    }
    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_font(HtmlWriter &out) const {
    HtmlResources resources;

    const auto program = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
        m_font_file.impl()->font_program());

    out.write_begin();
    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_end();

    out.write_body_begin();

    if (program == nullptr) {
      out.out() << "<p>unsupported font</p>";
      out.write_body_end();
      out.write_end();
      return resources;
    }

    // Re-encode for the browser and embed it; every glyph is then addressable
    // at its deterministic PUA code point.
    std::string reencoded;
    try {
      reencoded = font::reencode_to_pua(*program);
    } catch (...) {
      out.out() << "<p>font has too many glyphs to display</p>";
      out.write_body_end();
      out.write_end();
      return resources;
    }
    const std::string mime = m_font_file.file_type() == FileType::opentype_font
                                 ? "font/otf"
                                 : "font/ttf";
    const std::string url = file_to_url(reencoded, mime);

    out.out() << "<style>";
    out.out() << "@font-face{font-family:'odr-specimen';src:url(" << url
              << ");}";
    out.out() << "body{font-family:sans-serif;}";
    out.out() << ".grid{display:flex;flex-wrap:wrap;}";
    out.out() << ".cell{width:4em;height:4em;border:1px solid #ddd;margin:2px;"
                 "display:flex;flex-direction:column;align-items:center;"
                 "justify-content:center;}";
    out.out()
        << ".glyph{font-family:'odr-specimen';font-size:2em;line-height:1;}";
    out.out() << ".gid{font-size:.6em;color:#888;}";
    out.out() << "</style>";

    out.out() << "<h1>" << escape(program->name()) << "</h1>";
    out.out() << "<p>glyphs: " << program->glyph_count()
              << " &middot; units/em: " << program->units_per_em()
              << " &middot; format: "
              << (program->format() == FontFormat::opentype_cff ? "OpenType/CFF"
                                                                : "TrueType")
              << (program->symbolic() ? " &middot; symbolic" : "") << "</p>";

    const std::uint16_t shown =
        std::min<std::uint16_t>(program->glyph_count(), max_grid_glyphs);
    out.out() << "<div class=\"grid\">";
    for (std::uint16_t gid = 1; gid < shown; ++gid) {
      out.out() << "<div class=\"cell\"><span class=\"glyph\">&#x" << std::hex
                << static_cast<std::uint32_t>(font::pua_code_point(gid))
                << std::dec << ";</span><span class=\"gid\">" << gid;
      if (const auto cp = program->code_point_for_glyph(gid)) {
        out.out() << " U+" << std::hex << std::uppercase
                  << static_cast<std::uint32_t>(*cp) << std::nouppercase
                  << std::dec;
      }
      out.out() << "</span></div>";
    }
    out.out() << "</div>";

    if (program->glyph_count() > shown) {
      out.out() << "<p>(showing first " << shown << " glyphs)</p>";
    }

    out.write_body_end();
    out.write_end();

    return resources;
  }

protected:
  FontFile m_font_file;
  HtmlViews m_views;
};

} // namespace

odr::HtmlService create_font_service(const FontFile &font_file,
                                     const std::string & /*cache_path*/,
                                     HtmlConfig config,
                                     std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      font_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal::html
