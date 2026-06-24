#include <odr/internal/html/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf/pdf_page_text.hpp>
#include <odr/internal/util/string_util.hpp>

#include <utf8cpp/utf8/unchecked.h>

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
  struct PageOut {
    std::string classes;
    std::vector<SpanOut> spans;
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

    // Round CSS coordinates to 0.01px; sub-pixel precision beyond that is
    // invisible and the extra digits add up over millions of spans.
    const auto round2 = [](const double v) {
      return std::round(v * 100.0) / 100.0;
    };

    // Pass 1: resolve every page and span into class tokens, building the
    // atomic-style catalog so it can be emitted in <head> ahead of the body.
    AtomicStyles styles;
    std::vector<PageOut> pages_out;
    pages_out.reserve(pages.size());

    // Embedded fonts get a PUA re-encode, an `@font-face`, and a `font-family`
    // class `odr-fN`, assigned on first encounter in `font_family`. A font
    // whose embedded font is absent, not an SFNT, or not re-encodable keeps
    // index 0 and renders through the fallback path, exactly as before.
    // `font_faces` collects the rules for the accepted fonts, emitted in <head>
    // below.
    int family_count = 0;
    std::string font_faces;
    std::unordered_map<const pdf::Font *, int> family_index;
    const auto font_family = [&](pdf::Font *font) -> int {
      const auto [it, inserted] = family_index.try_emplace(font, 0);
      if (!inserted) {
        return it->second; // already classified
      }
      auto sfnt =
          std::dynamic_pointer_cast<font::sfnt::SfntFont>(font->embedded_font);
      if (sfnt == nullptr) {
        return 0; // no usable embedded font: fallback path
      }
      // Re-encode to the PUA up front so taking the embedded-font path is gated
      // on success: a font we cannot re-encode (more glyphs than the BMP PUA
      // holds, or a serialization failure) keeps index 0 and renders through
      // the legible fallback path, never emitting orphaned PUA glyph spans. The
      // re-encode mutates the cmap in place, but `glyph_for_code` reads it
      // during extraction below, so snapshot the original cmap and restore it
      // after.
      std::string reencoded;
      std::map<char32_t, std::uint16_t> original_cmap = sfnt->cmap();
      try {
        font::reencode_to_pua(*sfnt);
        std::ostringstream sfnt_out;
        sfnt->write(sfnt_out);
        reencoded = std::move(sfnt_out).str();
      } catch (...) {
        reencoded.clear();
      }
      sfnt->set_cmap(std::move(original_cmap));
      if (reencoded.empty()) {
        return 0; // not re-encodable: fallback path
      }
      const int index = ++family_count;
      it->second = index;
      const std::string url = file_to_url(reencoded, "font/ttf");
      font_faces += "@font-face{font-family:'odr-f" + std::to_string(index) +
                    "';src:url(" + url + ");}";
      return index;
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

      for (const pdf::TextElement &text :
           pdf::extract_text(stream, *page->resources, *m_logger)) {
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
             util::string::utf8_length(text.text) ==
                 static_cast<std::ptrdiff_t>(text.advances.size()));
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

        if (font != 0) {
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
            page_out.spans.push_back({base + " i", escape_text(text.text),
                                      std::move(glyph_classes),
                                      std::move(glyph_text)});
          } else {
            // Display-only run: nothing is extractable (the `no_unicode` case),
            // so the glyph layer stands alone and carries the placement itself.
            std::string glyph_classes = base + " g";
            glyph_classes += paint;
            add_class(glyph_classes, "ff", font_family);
            page_out.spans.push_back(
                {std::move(glyph_classes), std::move(glyph_text), {}, {}});
          }
        } else {
          // Legacy single-layer path: no embedded font, render the Unicode in a
          // fallback font.
          std::string classes = base;
          if (invisible) {
            classes += " i";
          }
          page_out.spans.push_back(
              {std::move(classes), escape_text(text.text), {}, {}});
        }
      }
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
    out.out() << ".p{position:relative}";
    out.out() << ".t{position:absolute;left:0;top:0;transform-origin:0 0;"
                 "white-space:pre}";
    // Invisible text render modes (Tr 3/7): kept in the DOM for selection and
    // search (OCR-over-scan), but not painted.
    out.out() << ".i{color:transparent}";
    // The visible glyph layer: not selectable, so the enclosing text layer
    // owns copy/search and the PUA code points stay off the clipboard. `.gv`
    // paints it; set explicitly because the nested layer would otherwise
    // inherit the `.i` text layer's `transparent`.
    out.out() << ".g{user-select:none}.gv{color:#000}";
    // Embedded fonts, re-encoded to the PUA and served inline.
    out.out() << font_faces;
    // Per-value atomic classes (font sizes, offsets, transforms, ...).
    styles.write_rules(out.out());
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();
    for (const PageOut &page : pages_out) {
      out.write_element_begin("div",
                              HtmlElementOptions().set_class(page.classes));
      for (const SpanOut &span : page.spans) {
        // Inline so the whole run (and its nested glyph layer) stays on one
        // line: smaller output and a more legible diff than the open/text/close
        // split, while each run still gets its own line under the page div.
        out.write_element_begin(
            "span",
            HtmlElementOptions().set_inline(true).set_class(span.classes));
        out.write_raw(span.text);
        if (!span.glyph_classes.empty()) {
          out.write_element_begin(
              "span", HtmlElementOptions().set_inline(true).set_class(
                          span.glyph_classes));
          out.write_raw(span.glyph_text);
          out.write_element_end("span");
        }
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
