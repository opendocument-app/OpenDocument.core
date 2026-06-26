#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/cff_transform.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_page_extractor.hpp>
#include <odr/internal/util/string_util.hpp>

#include <utf8cpp/utf8/unchecked.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::html {

namespace {

/// Round to 0.01 user-space units; sub-precision beyond that is invisible and
/// the extra digits add up across a page full of path data.
double round2(const double v) { return std::round(v * 100.0) / 100.0; }

/// Convert a PDF device color to a CSS `rgb(...)` string. Non-device color
/// spaces (Separation/ICCBased/… — stage 4.4) and the unknown space fall back
/// to black, the PDF initial color.
std::string device_color_to_css(const pdf::GraphicsState::Color &color) {
  const auto to255 = [](const double v) {
    return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0));
  };
  int r = 0;
  int g = 0;
  int b = 0;
  switch (color.space) {
  case pdf::ColorSpace::device_grey:
    r = g = b = to255(color.grey);
    break;
  case pdf::ColorSpace::device_rgb:
    r = to255(color.rgb[0]);
    g = to255(color.rgb[1]);
    b = to255(color.rgb[2]);
    break;
  case pdf::ColorSpace::device_cmyk: {
    // Naive CMYK -> RGB (no ICC); refined in stage 4.4.
    const double c = color.cmyk[0];
    const double m = color.cmyk[1];
    const double y = color.cmyk[2];
    const double k = color.cmyk[3];
    r = to255((1 - c) * (1 - k));
    g = to255((1 - m) * (1 - k));
    b = to255((1 - y) * (1 - k));
    break;
  }
  case pdf::ColorSpace::unknown:
    break;
  }
  std::ostringstream s;
  s << "rgb(" << r << ',' << g << ',' << b << ')';
  return std::move(s).str();
}

/// Build an SVG `d` attribute from a path's subpaths, each point mapped through
/// `to_box` (PDF user space -> the page box, y-down). Lines become `L`, cubic
/// Béziers `C`, and an explicitly closed subpath ends with `Z`.
std::string svg_path_d(const std::vector<pdf::Subpath> &subpaths,
                       const util::math::Transform2D &to_box) {
  std::ostringstream d;
  const auto point = [&](const std::array<double, 2> &p) {
    const std::array<double, 2> q = to_box.apply(p[0], p[1]);
    d << ' ' << round2(q[0]) << ' ' << round2(q[1]);
  };
  bool first = true;
  for (const pdf::Subpath &sub : subpaths) {
    d << (first ? "M" : " M");
    first = false;
    point(sub.start);
    for (const pdf::PathSegment &seg : sub.segments) {
      if (seg.kind == pdf::PathSegment::Kind::line) {
        d << " L";
        point(seg.end);
      } else {
        d << " C";
        point(seg.c1);
        point(seg.c2);
        point(seg.end);
      }
    }
    if (sub.closed) {
      d << " Z";
    }
  }
  return std::move(d).str();
}

/// Serialize a painted path to an SVG `<path .../>` fragment in the page
/// viewBox, or "" when it paints nothing. Fill honours the even-odd rule;
/// stroke carries width (CTM-scaled in user space), caps, joins, miter limit
/// and the dash pattern. A zero stroke width renders as a thin hairline.
std::string svg_path_fragment(const pdf::PathElement &path,
                              const util::math::Transform2D &to_box) {
  if ((!path.fill && !path.stroke) || path.subpaths.empty()) {
    return {};
  }
  std::ostringstream f;
  f << "<path d=\"" << svg_path_d(path.subpaths, to_box) << '"';

  if (path.fill) {
    f << " fill=\"" << device_color_to_css(path.fill_color) << '"';
    if (path.even_odd) {
      f << " fill-rule=\"evenodd\"";
    }
  } else {
    f << " fill=\"none\"";
  }

  if (path.stroke) {
    f << " stroke=\"" << device_color_to_css(path.stroke_color) << '"';
    // A 0 width is "device-thinnest" in PDF; SVG would draw nothing, so floor
    // it to a sub-point hairline.
    const double width = path.line_width > 0 ? path.line_width : 0.5;
    f << " stroke-width=\"" << round2(width) << '"';
    if (path.line_cap == 1) {
      f << " stroke-linecap=\"round\"";
    } else if (path.line_cap == 2) {
      f << " stroke-linecap=\"square\"";
    }
    if (path.line_join == 1) {
      f << " stroke-linejoin=\"round\"";
    } else if (path.line_join == 2) {
      f << " stroke-linejoin=\"bevel\"";
    } else {
      // miter join: SVG defaults the limit to 4, PDF to 10 — state it.
      f << " stroke-miterlimit=\"" << round2(path.miter_limit) << '"';
    }
    const bool dashed =
        std::any_of(path.dash_array.begin(), path.dash_array.end(),
                    [](const double v) { return v > 0; });
    if (dashed) {
      f << " stroke-dasharray=\"";
      for (std::size_t i = 0; i < path.dash_array.size(); ++i) {
        f << (i == 0 ? "" : ",") << round2(path.dash_array[i]);
      }
      f << '"';
      if (path.dash_phase != 0) {
        f << " stroke-dashoffset=\"" << round2(path.dash_phase) << '"';
      }
    }
  }

  f << "/>";
  return std::move(f).str();
}

/// Deduplicates CSS declarations into atomic, single-property classes. PDF text
/// emits one absolutely-positioned span per glyph run, and the same font sizes,
/// offsets and spacings recur across the (potentially millions of) spans.
/// Writing each declaration inline bloats the document — the Bluetooth Core
/// spec reference output crossed GitHub's 100 MB file limit. Instead, every
/// distinct declaration is registered once here, named `<prefix><n>` in
/// first-seen order (e.g. `f1`, `f2` for font sizes, `t1` for a top offset),
/// emitted once in <head>, and referenced by class on each span. This is
/// representation-only: the computed style of every element is unchanged.
class AtomicStyles {
public:
  /// `prefix` selects the property family; `declaration` is a full CSS
  /// declaration without trailing ';' (e.g. "font-size:13.28px"). Returns the
  /// class name to add to the element.
  const std::string &intern(const std::string &prefix,
                            std::string declaration) {
    const auto [it, inserted] =
        m_class_by_declaration.try_emplace(std::move(declaration));
    if (inserted) {
      it->second = prefix + std::to_string(++m_count_by_prefix[prefix]);
      m_order.push_back(&*it);
    }
    return it->second;
  }

  /// Writes one rule per line (`.f1{font-size:13.28px}`) so regeneration diffs
  /// stay legible. Each rule is preceded by a newline; the caller has already
  /// written the constant rules on the opening `<style>` line.
  void write_rules(std::ostream &o) const {
    for (const auto *entry : m_order) {
      o << "\n." << entry->second << '{' << entry->first << '}';
    }
  }

private:
  // Node-based map: pointers stored in `m_order` stay valid across insertions.
  std::unordered_map<std::string, std::string> m_class_by_declaration;
  std::unordered_map<std::string, int> m_count_by_prefix;
  std::vector<const std::pair<const std::string, std::string> *> m_order;
};

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

  // One emitted span. The styling is fully resolved into class tokens during
  // the first pass; only the (already escaped) text and class list survive to
  // the writing pass. A text run with an embedded font emits the dual layer as
  // a transparent selectable span carrying the real Unicode with the visible
  // glyph layer (PUA code points in the `@font-face` font) nested inside it:
  // the child is absolutely positioned at the run origin and inherits the
  // font size, spacing, and transform from the parent, so the placement
  // classes live only on the parent. `glyph_classes` is empty when there is no
  // nested layer (the legacy fallback path and display-only runs).
  struct SpanOut {
    std::string classes;
    std::string text;
    std::string glyph_classes;
    std::string glyph_text;
  };
  // A text run with an embedded font that *might* collapse to a single span (it
  // is 1:1 and carries real Unicode). Whether it actually collapses depends on
  // the font's final re-encoded `cmap`, which is only known once every page has
  // been scanned (a glyph wins a real-Unicode entry only if no earlier glyph
  // claimed that scalar first). So these runs are buffered raw and resolved
  // into a `SpanOut` — collapsed or dual — after the first pass. `pdf_font` and
  // `codes` re-derive the per-code glyphs; `text` is the raw (unescaped)
  // Unicode, escaped at resolution.
  struct PendingText {
    std::string base; // placement + spacing classes
    int font{0};      // `@font-face` index (>= 1)
    bool invisible{false};
    std::string text;
    std::string codes;
    const pdf::Font *pdf_font{nullptr};
  };
  // One painted path, already serialized to an SVG `<path .../>` fragment in
  // the page's viewBox (PDF points, y-down). Contiguous paths share one `<svg>`
  // at write time.
  struct PathOut {
    std::string svg;
  };
  // Page content in paint (z) order: text spans and paths interleave, so a
  // later fill occludes earlier text and vice versa. `PendingText` survives
  // only until the post-pass resolution rewrites it to a `SpanOut`.
  using PageItem = std::variant<SpanOut, PathOut, PendingText>;
  struct PageOut {
    std::string classes;
    double width{0};  // page box width, PDF points (for the SVG viewBox)
    double height{0}; // page box height, PDF points
    std::vector<PageItem> items;
  };

  HtmlResources write_document(HtmlWriter &out) const {
    HtmlResources resources;

    const auto &pdf_file =
        dynamic_cast<const pdf::PdfFile &>(*m_pdf_file.impl());
    pdf::DocumentParser parser = pdf_file.create_parser(*m_logger);
    const std::unique_ptr<pdf::Document> document = parser.parse_document();

    const std::vector<pdf::Page *> pages = document->collect_pages();

    // CSS uses 96px to the inch, PDF user space 72 units to the inch.
    static constexpr double pt_to_px = 96.0 / 72.0;
    static constexpr double pt_to_in = 1 / 72.0;

    // Pass 1: resolve every page and span into class tokens, building the
    // atomic-style catalog so it can be emitted in <head> ahead of the body.
    AtomicStyles styles;
    std::vector<PageOut> pages_out;
    pages_out.reserve(pages.size());

    // Embedded fonts get a PUA re-encode, an `@font-face`, and a `font-family`
    // class `odr-fN`, assigned on first encounter in `font_family`. A font
    // whose embedded font is absent, not an SFNT, or not re-encodable keeps
    // index 0 and renders through the fallback path, exactly as before.
    //
    // The `@font-face` rules are *not* built here: a font also gets
    // real-Unicode `cmap` entries for the scalars its 1:1 runs use (so those
    // runs can collapse to a single span), and that used-scalar set is only
    // complete after the first pass. `font_family` therefore just validates and
    // indexes the font; `accepted_fonts` / `used_unicode` (indexed by `index -
    // 1`) carry it to the post-pass that re-encodes with the extra entries and
    // emits `font_faces`.
    int family_count = 0;
    std::string font_faces;
    std::vector<pdf::Font *> accepted_fonts;
    std::vector<std::map<char32_t, std::uint16_t>> used_unicode;
    std::unordered_map<const pdf::Font *, int> family_index;
    const auto font_family = [&](pdf::Font *font) -> int {
      const auto [it, inserted] = family_index.try_emplace(font, 0);
      if (!inserted) {
        return it->second; // already classified
      }
      // Gate the embedded-font path on a trial PUA re-encode + serialize: a
      // font we cannot re-encode (more glyphs than the BMP PUA holds, or a
      // serialization failure) keeps index 0 and renders through the legible
      // fallback path, never emitting orphaned PUA glyph spans. The trial
      // output is discarded; the post-pass re-encodes for real (with the
      // real-Unicode entries) once the used-scalar set is known.
      bool usable = false;
      if (auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
              font->embedded_font)) {
        // SFNT (TrueType / OpenType): the re-encode mutates the cmap in place,
        // but `glyph_for_code` reads it during extraction below, so snapshot
        // the original cmap and restore it after.
        std::map<char32_t, std::uint16_t> original_cmap = sfnt->cmap();
        try {
          font::reencode_to_pua(*sfnt);
          std::ostringstream sfnt_out;
          sfnt->write(sfnt_out);
          usable = true;
        } catch (...) {
          usable = false;
        }
        sfnt->set_cmap(std::move(original_cmap));
      } else if (auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                     font->embedded_font)) {
        // Bare CFF (`/FontFile3`): wrap into an OTTO with the PUA cmap baked in
        // (no in-place mutation, so nothing to restore).
        try {
          (void)font::cff::wrap_to_otf(*cff);
          usable = true;
        } catch (...) {
          usable = false;
        }
      }
      if (!usable) {
        return 0; // no usable embedded font: fallback path
      }
      const int index = ++family_count;
      it->second = index;
      accepted_fonts.push_back(font);
      used_unicode.emplace_back();
      return index;
    };

    // A real-Unicode scalar may carry a `cmap` entry (letting its run collapse)
    // only inside the BMP and outside the PUA (`U+E000..U+F8FF`), so a glyph's
    // own deterministic PUA code point (`pua_code_point`) is never shadowed.
    const auto collapsible_unicode = [](const char32_t c) {
      return c <= 0xFFFF && !(c >= 0xE000 && c <= 0xF8FF);
    };

    // The PUA glyph string for a run: each character code -> glyph id ->
    // deterministic PUA code point (`U+E000 + glyph`), matching the re-encode.
    const auto glyph_run = [](const pdf::Font &font, const std::string &codes) {
      std::string new_codes;
      for (const std::uint32_t code : font.codes(codes)) {
        util::string::append_c32(
            font::pua_code_point(font.glyph_for_code(code)), new_codes);
      }
      return new_codes;
    };

    // Appends `prefix:value` (interned) as a class token on `classes`.
    const auto add_class = [&styles](std::string &classes,
                                     const std::string &prefix,
                                     std::string declaration) {
      classes += ' ';
      classes += styles.intern(prefix, std::move(declaration));
    };
    const auto px_decl = [](const char *property, const double value) {
      std::ostringstream s;
      s << property << ':' << value << "px";
      return std::move(s).str();
    };

    for (pdf::Page *page : pages) {
      const pdf::Array &page_box = page->media_box.as_array();
      const double box_x0 = page_box[0].as_real();
      const double box_y0 = page_box[1].as_real();
      const double width = page_box[2].as_real() - box_x0;
      const double height = page_box[3].as_real() - box_y0;

      PageOut &page_out = pages_out.emplace_back();
      page_out.classes = "p";
      page_out.width = width;
      page_out.height = height;
      {
        std::ostringstream w;
        w << "width:" << width * pt_to_in << "in";
        add_class(page_out.classes, "x", std::move(w).str());
        std::ostringstream h;
        h << "height:" << height * pt_to_in << "in";
        add_class(page_out.classes, "y", std::move(h).str());
      }

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
          util::math::Transform2D::scaling_translation(1, -1, 0, height);

      for (const pdf::PageElement &element :
           pdf::extract_page(stream, *page->resources, *m_logger)) {
        // A painted path: serialize its subpaths to an SVG `<path>` fragment in
        // the page viewBox (fill and/or stroke).
        if (const auto *path = std::get_if<pdf::PathElement>(&element)) {
          std::string fragment = svg_path_fragment(*path, to_box);
          if (!fragment.empty()) {
            page_out.items.push_back(PathOut{std::move(fragment)});
          }
          continue;
        }

        const pdf::TextElement &text = std::get<pdf::TextElement>(element);
        // The font index is non-zero when an embedded font lets us render
        // the actual glyphs; 0 falls through to the legacy path.
        const int font = text.font != nullptr ? font_family(text.font) : 0;
        // Without an embedded font, an empty `text` has nothing to show: a code
        // with no recoverable Unicode (`no_unicode`) or an `/ActualText`-
        // suppressed segment. With one, the glyphs still render (PUA layer).
        if (text.text.empty() && font == 0) {
          continue;
        }

        const util::math::Transform2D m = flip_glyph * text.transform * to_box;

        // Tr 3 (invisible) and Tr 7 (clip-only) paint nothing; keep them
        // selectable via the transparent `.i` class.
        const bool invisible =
            text.rendering_mode == 3 || text.rendering_mode == 7;

        // Placement and spacing are shared by both layers of a run; build them
        // once on `base`.
        std::string base = "t";
        // TODO baseline sits at the box top until font ascent metrics land

        // Tc/Tw are absolute text-space lengths (not scaled by the font size).
        // One text-space unit is `scale * pt_to_px` CSS px, where `scale` is
        // the linear factor we apply to the glyphs: folded into `font-size` in
        // the uniform branch, carried by the CSS matrix in the general branch
        // (so spacing there is expressed pre-transform, scale == 1).
        double scale;
        if (m.b == 0 && m.c == 0 && m.a == m.d) {
          // Upright uniform scale: fold the scale into the font size and place
          // the origin with left/top, so the (otherwise near-universal) matrix
          // is dropped.
          add_class(base, "l", px_decl("left", round2(m.e * pt_to_px)));
          add_class(base, "t", px_decl("top", round2(m.f * pt_to_px)));
          add_class(base, "f",
                    px_decl("font-size", round2(m.a * text.size * pt_to_px)));
          scale = m.a;
        } else {
          std::ostringstream tm;
          tm << "transform:matrix(" << m.a << "," << m.b << "," << m.c << ","
             << m.d << "," << round2(m.e * pt_to_px) << ","
             << round2(m.f * pt_to_px) << ")";
          add_class(base, "m", std::move(tm).str());
          add_class(base, "f",
                    px_decl("font-size", round2(text.size * pt_to_px)));
          scale = 1;
        }

        // PDF char/word spacing (Tc/Tw) translate directly to CSS. TJ kerning
        // needs no expression here: `extract_text` emits a separate segment per
        // TJ string and folds the adjustment into the following segment's
        // `transform`, so a segment only carries its constant spacing. Emitted
        // only when non-zero to keep the (overwhelmingly common) unspaced span
        // small.
        //
        // CSS letter-/word-spacing key on the *rendered* string's character and
        // space boundaries, but PDF Tc/Tw advance the text matrix per raw code
        // (Tw only on a simple font's single-byte 0x20; ISO 32000-1 9.3.3). The
        // two coincide only when the rendered run is 1:1 with the codes. The
        // glyph layer always is (one PUA code point per code, `font != 0`); the
        // Unicode text layer is not when a /ToUnicode CMap expands a code into
        // several characters (ligatures), /ActualText substitutes text, or a
        // space was inferred — there CSS would insert gaps the segment advances
        // never accounted for, splitting glyphs and drifting the next
        // absolutely-positioned segment. Gate emission on that correspondence;
        // word spacing additionally never applies to a composite font.
        const bool spacing_one_to_one =
            font != 0 ||
            (text.font != nullptr &&
             util::string::utf8_length(text.text) == text.advances.size());
        if (text.char_spacing != 0 && spacing_one_to_one) {
          add_class(base, "s",
                    px_decl("letter-spacing",
                            round2(text.char_spacing * scale * pt_to_px)));
        }
        if (text.word_spacing != 0 && spacing_one_to_one &&
            !(text.font != nullptr && text.font->composite)) {
          add_class(base, "w",
                    px_decl("word-spacing",
                            round2(text.word_spacing * scale * pt_to_px)));
        }

        // A run collapses to a single span — selectable *and* visible, the real
        // Unicode rendered directly in the embedded font — when it has an
        // embedded font, carries text, and is 1:1 with its codes (no /ToUnicode
        // expansion, /ActualText, or inferred space). Whether it actually
        // collapses also needs every glyph to win a real-Unicode `cmap` entry,
        // resolved in the post-pass; defer such runs as `PendingText`.
        const bool collapse_candidate =
            font != 0 && !text.text.empty() && text.font != nullptr &&
            util::string::utf8_length(text.text) == text.advances.size();

        if (collapse_candidate) {
          // Stake first-wins real-Unicode -> glyph claims for this run's
          // scalars. The post-pass bakes the winners into the font's `cmap` and
          // decides per run whether it collapses or falls back to the dual
          // layer (a later glyph that lost its scalar stays dual).
          std::map<char32_t, std::uint16_t> &won = used_unicode[font - 1];
          auto cp = text.text.begin();
          for (const std::uint32_t code : text.font->codes(text.codes)) {
            const char32_t uchar = utf8::unchecked::next(cp);
            if (collapsible_unicode(uchar)) {
              won.emplace(uchar, text.font->glyph_for_code(code));
            }
          }
          page_out.items.push_back(PendingText{std::move(base), font, invisible,
                                               text.text, text.codes,
                                               text.font});
        } else if (font != 0) {
          // The visible glyph layer: PUA code points in the embedded font.
          // Painted unless the render mode is invisible; never selected (the
          // text layer owns selection via `.g`), so the PUA code points never
          // reach the clipboard.
          const std::string font_family =
              "font-family:'odr-f" + std::to_string(font) + "'";
          std::string glyph_text =
              escape_text(glyph_run(*text.font, text.codes));
          // The glyph layer paints (opaque `.gv`) unless the render mode is
          // invisible (transparent `.i`). The painted color is set explicitly
          // because, when nested, the layer would otherwise inherit the
          // `.i` text layer's `transparent`.
          const char *const paint = invisible ? " i" : " gv";

          if (!text.text.empty()) {
            // Dual layer: a transparent selectable span carrying the real
            // Unicode (for copy/search) with the glyph layer nested inside.
            // The nested `.t` child overlays the run origin and inherits the
            // placement, so only `g`/paint/`ff` need restating on it.
            std::string glyph_classes = "t g";
            glyph_classes += paint;
            add_class(glyph_classes, "ff", font_family);
            page_out.items.push_back(
                SpanOut{base + " i", escape_text(text.text),
                        std::move(glyph_classes), std::move(glyph_text)});
          } else {
            // Display-only run: nothing is extractable (the `no_unicode` case),
            // so the glyph layer stands alone and carries the placement itself.
            std::string glyph_classes = base + " g";
            glyph_classes += paint;
            add_class(glyph_classes, "ff", font_family);
            page_out.items.push_back(SpanOut{
                std::move(glyph_classes), std::move(glyph_text), {}, {}});
          }
        } else {
          // Legacy single-layer path: no embedded font, render the Unicode in a
          // fallback font.
          std::string classes = base;
          if (invisible) {
            classes += " i";
          }
          page_out.items.push_back(
              SpanOut{std::move(classes), escape_text(text.text), {}, {}});
        }
      }
    }

    // Post-pass: every page has been scanned, so the per-font used-scalar sets
    // are complete. First resolve each deferred `PendingText` into a collapsed
    // or dual `SpanOut` (this reads `glyph_for_code` on the *original* cmaps,
    // so it must run before the in-place SFNT re-encode below), then re-encode
    // the fonts with their real-Unicode entries and emit the `@font-face`
    // rules.
    const auto resolve_pending = [&](const PendingText &pt) -> SpanOut {
      const std::map<char32_t, std::uint16_t> &won = used_unicode[pt.font - 1];
      const std::string font_family =
          "font-family:'odr-f" + std::to_string(pt.font) + "'";

      // Collapse iff every code's glyph won this run's scalar; the walk also
      // re-checks the exact 1:1 correspondence.
      bool collapse = true;
      auto cp = pt.text.begin();
      const auto cp_end = pt.text.end();
      for (const std::uint32_t code : pt.pdf_font->codes(pt.codes)) {
        if (cp == cp_end) {
          collapse = false;
          break;
        }
        const char32_t uchar = utf8::unchecked::next(cp);
        const auto entry = won.find(uchar);
        if (entry == won.end() ||
            entry->second != pt.pdf_font->glyph_for_code(code)) {
          collapse = false;
          break;
        }
      }
      if (cp != cp_end) {
        collapse = false;
      }

      if (collapse) {
        // One span: the real Unicode rendered in the embedded font, visible
        // (`.gv` black) or transparent (`.i`), and selectable either way.
        std::string classes = pt.base;
        classes += pt.invisible ? " i" : " gv";
        add_class(classes, "ff", font_family);
        return SpanOut{std::move(classes), escape_text(pt.text), {}, {}};
      }
      // Dual layer (a glyph lost its scalar to an earlier one): a transparent
      // selectable Unicode span with the PUA glyph layer nested inside.
      std::string glyph_classes = "t g";
      glyph_classes += pt.invisible ? " i" : " gv";
      add_class(glyph_classes, "ff", font_family);
      return SpanOut{pt.base + " i", escape_text(pt.text),
                     std::move(glyph_classes),
                     escape_text(glyph_run(*pt.pdf_font, pt.codes))};
    };
    for (PageOut &page_out : pages_out) {
      for (PageItem &item : page_out.items) {
        if (const auto *pending = std::get_if<PendingText>(&item)) {
          item = resolve_pending(*pending);
        }
      }
    }

    // Re-encode each accepted font with its real-Unicode entries baked into the
    // `cmap` (the PUA range is kept as a fallback) and emit the `@font-face`
    // rules in index order, so the output stays deterministic.
    for (int i = 0; i < family_count; ++i) {
      pdf::Font *font = accepted_fonts[i];
      const std::map<char32_t, std::uint16_t> &extra = used_unicode[i];
      std::string reencoded;
      if (auto sfnt = std::dynamic_pointer_cast<font::sfnt::SfntFont>(
              font->embedded_font)) {
        font::reencode_to_pua(*sfnt, extra);
        std::ostringstream sfnt_out;
        sfnt->write(sfnt_out);
        reencoded = std::move(sfnt_out).str();
      } else if (auto cff = std::dynamic_pointer_cast<font::cff::CffFont>(
                     font->embedded_font)) {
        reencoded = font::cff::wrap_to_otf(*cff, extra);
      }
      const std::string url = file_to_url(reencoded, "font/ttf");
      font_faces += "@font-face{font-family:'odr-f" + std::to_string(i + 1) +
                    "';src:url(" + url + ");}";
    }

    // Pass 2: write the document, now that the catalog is complete.
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
    // Page presentation: a neutral backdrop with each page as a centered white
    // sheet and a soft drop shadow, mirroring the familiar PDF-viewer look.
    // This is purely cosmetic chrome around the page box; the
    // absolutely-positioned spans inside are unaffected (they anchor to the
    // `.p` box, which keeps `position:relative`).
    out.out() << "body{margin:0;background:#525659}";
    out.out() << ".p{position:relative;margin:16px auto;background:#fff;"
                 "box-shadow:0 1px 4px rgba(0,0,0,.5)}";
    // `font-kerning:none` + `font-variant-ligatures:none` keep the browser from
    // applying the embedded font's GPOS/GSUB tables. A collapsed run now emits
    // real Unicode in that font, so without this a sequence like `fi`/`AV`
    // could be re-shaped (ligature substitution, kerning) after this code
    // already fixed the PDF glyph IDs and advances, shifting pixels and run
    // widths for otherwise 1:1 text. The PUA glyph layer was immune; restore
    // that here.
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre;font-kerning:none;"
                 "font-variant-ligatures:none}";
    // Invisible text render modes (Tr 3/7): kept in the DOM for selection and
    // search (OCR-over-scan), but not painted.
    out.out() << ".i{color:transparent}";
    // The visible glyph layer: not selectable, so the enclosing text layer
    // owns copy/search and the PUA code points stay off the clipboard. `.gv`
    // paints it; set explicitly because the nested layer would otherwise
    // inherit the `.i` text layer's `transparent`.
    out.out() << ".g{user-select:none}.gv{color:#000}";
    // Vector graphics: one or more `<svg>` overlays per page, each filling the
    // page box (viewBox in PDF points). `overflow:hidden` clips each overlay to
    // the page box, matching a PDF viewer: content drawn outside the MediaBox
    // (e.g. a background rectangle that bleeds past the left edge) is never
    // visible, and without this it spills into the centered page's margin.
    // Arbitrary in-page clip paths still wait for stage 4.3.
    // `preserveAspectRatio:none` keeps the points->box mapping exact.
    // `pointer-events:none` so a full-page overlay painted after text
    // (paint order) does not swallow selection/clicks over its transparent
    // areas — the graphics are decorative, the text layer owns interaction.
    out.out() << ".s{position:absolute;left:0;top:0;width:100%;height:100%;"
                 "overflow:hidden;pointer-events:none}";
    // Embedded fonts, re-encoded to the PUA and served inline.
    out.out() << font_faces;
    // Per-value atomic classes (font sizes, offsets, transforms, ...).
    styles.write_rules(out.out());
    out.write_header_style_end();
    out.write_header_end();

    const auto write_span = [&out](const SpanOut &span) {
      // Inline so the whole run (and its nested glyph layer) stays on one line:
      // smaller output and a more legible diff than the open/text/close split,
      // while each run still gets its own line under the page div.
      out.write_element_begin(
          "span",
          HtmlElementOptions().set_inline(true).set_class(span.classes));
      out.write_raw(span.text);
      if (!span.glyph_classes.empty()) {
        out.write_element_begin("span",
                                HtmlElementOptions().set_inline(true).set_class(
                                    span.glyph_classes));
        out.write_raw(span.glyph_text);
        out.write_element_end("span");
      }
      out.write_element_end("span");
    };

    out.write_body_begin();
    for (const PageOut &page : pages_out) {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class(page.classes));
      // Walk the page's elements in paint order, coalescing contiguous paths
      // into a single `<svg>` so spans and vector graphics layer by DOM order.
      bool svg_open = false;
      const auto close_svg = [&] {
        if (svg_open) {
          out.write_raw("</svg>");
          svg_open = false;
        }
      };
      for (const PageItem &item : page.items) {
        if (const auto *path = std::get_if<PathOut>(&item)) {
          if (!svg_open) {
            std::ostringstream open;
            open << "<svg class=\"s\" viewBox=\"0 0 " << round2(page.width)
                 << ' ' << round2(page.height)
                 << "\" preserveAspectRatio=\"none\">";
            out.write_raw(std::move(open).str());
            svg_open = true;
          }
          out.write_raw(path->svg);
        } else {
          close_svg();
          write_span(std::get<SpanOut>(item));
        }
      }
      close_svg();
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
