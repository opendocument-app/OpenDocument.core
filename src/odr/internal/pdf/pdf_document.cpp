#include <odr/internal/pdf/pdf_document.hpp>

#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/pdf/pdf_cid.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/util/string_util.hpp>

#include <stdexcept>

namespace odr::internal::pdf {

namespace {

template <typename Collection>
void collect_pages_impl(const Pages &pages, Collection &out) {
  for (Element *kid : pages.kids) {
    if (kid->is<Pages>()) {
      collect_pages_impl(kid->as<Pages>(), out);
    } else if (kid->is<Page>()) {
      out.push_back(&kid->as<Page>());
    } else {
      throw std::runtime_error("unexpected element in PDF page tree");
    }
  }
}

} // namespace

std::vector<Page *> Document::collect_pages() const {
  std::vector<Page *> pages;
  collect_pages_impl(*catalog->pages, pages);
  return pages;
}

double Font::advance_width(const std::uint32_t code) const {
  if (composite) {
    if (const auto it = cid_widths.find(code); it != cid_widths.end()) {
      return it->second / 1000.0;
    }
    return cid_default_width / 1000.0;
  }
  const std::int64_t index = static_cast<std::int64_t>(code) - first_char;
  if (type3.has_value()) {
    // Type3 widths are in glyph space, mapped to text space by `/FontMatrix`
    // (ISO 32000-1 9.6.5) — not the fixed 1/1000 em of other fonts. The
    // horizontal advance is the x-component of `(w, 0)` through the matrix.
    if (index >= 0 && index < static_cast<std::int64_t>(widths.size())) {
      return widths[static_cast<std::size_t>(index)] * type3->font_matrix.a;
    }
    return 0;
  }
  if (index >= 0 && index < static_cast<std::int64_t>(widths.size())) {
    return widths[static_cast<std::size_t>(index)] / 1000.0;
  }
  // Non-embedded standard-14 fonts usually ship no `/Widths`: fall back to the
  // substitute's AFM metrics (ISO 32000-1 9.6.2.2) so placement stays correct.
  if (substitute.has_value() && substitute->metrics.has_value() &&
      code <= 0xFF) {
    const StandardFont metrics = *substitute->metrics;
    if (encoding.has_value()) {
      // An explicit `/Encoding` (a `/Differences` override or a base encoding)
      // names the glyph; look that name up only. The built-in code table
      // assumes the font's own code->glyph mapping, which the encoding
      // overrides, so consulting it on a miss would return an unrelated glyph's
      // width — fall through to `/MissingWidth` instead.
      if (const std::string_view name =
              encoding->glyph_name(static_cast<std::uint8_t>(code));
          !name.empty()) {
        if (const std::optional<double> width = afm_width(metrics, name);
            width.has_value()) {
          return *width / 1000.0;
        }
      }
    } else if (const std::optional<double> width =
                   afm_code_width(metrics, static_cast<std::uint8_t>(code));
               width.has_value()) {
      // No `/Encoding`: the font's built-in encoding (the AFM's own codes) maps
      // code->glyph — the Symbol/ZapfDingbats case (text fonts default to
      // StandardEncoding).
      return *width / 1000.0;
    }
  }
  return missing_width / 1000.0;
}

namespace {

/// Recovers Unicode through the embedded font’s code → glyph → Unicode map.
/// Keeps partial mappings; returns empty if none resolve.
std::string reverse_map_unicode(const Font &font, const std::string &codes) {
  if (font.embedded_font == nullptr) {
    return {};
  }
  std::string result;
  bool any = false;
  for (const std::uint32_t code : font.codes(codes)) {
    if (const std::optional<char32_t> cp =
            font.embedded_font->code_point_for_glyph(font.glyph_for_code(code));
        cp.has_value()) {
      util::string::append_c32(*cp, result);
      any = true;
    }
  }
  return any ? result : "";
}

} // namespace

std::uint16_t Font::glyph_for_code(const std::uint32_t code) const {
  if (embedded_font == nullptr) {
    return 0;
  }
  if (composite) {
    // The code is the CID (Identity-H/V). A CID-keyed CFF (`CIDFontType0C`)
    // carries the CID -> GID map in its own charset (ISO 32000-1 9.7.4.2);
    // `/CIDToGIDMap` is only defined for TrueType `CIDFontType2` and is
    // Identity for CIDFontType0, so route CID-keyed CFF through the CFF charset
    // first.
    if (const auto *cff =
            dynamic_cast<const font::cff::CffFont *>(embedded_font.get());
        cff != nullptr && cff->is_cid_keyed()) {
      return cff->glyph_for_cid(static_cast<std::uint16_t>(code));
    }
    if (cid_to_gid.empty()) {
      return static_cast<std::uint16_t>(code); // Identity
    }
    return code < cid_to_gid.size() ? cid_to_gid[code] : 0;
  }
  // Simple Type1/CFF (ISO 32000-1 9.6.6.2): the name selects the glyph in the
  // font program. Subset producers name glyphs `gidNNNNN`, which no glyph list
  // translates, so the charset is the only link that reaches them.
  if (encoding.has_value()) {
    if (const std::uint16_t glyph = embedded_font->glyph_for_name(
            encoding->glyph_name(static_cast<std::uint8_t>(code)));
        glyph != 0) {
      return glyph;
    }
  }
  // Simple TrueType (ISO 32000-1 9.6.6.4), best effort: the embedded cmap keyed
  // on the byte code first (symbolic (3,0)/(1,0) fonts), then on the code's
  // Unicode (via the /Encoding glyph name), then the code as a GID.
  if (const std::uint16_t glyph =
          embedded_font->glyph_for_code_point(static_cast<char32_t>(code));
      glyph != 0) {
    return glyph;
  }
  // A (3,0) subtable keys the byte code at U+F000 + code.
  if (embedded_font->symbolic()) {
    if (const std::uint16_t glyph =
            embedded_font->glyph_for_code_point(0xf000 + code);
        glyph != 0) {
      return glyph;
    }
  }
  if (encoding.has_value()) {
    const std::u16string unicode = glyph_name_to_unicode(
        encoding->glyph_name(static_cast<std::uint8_t>(code)));
    if (!unicode.empty()) {
      if (const std::uint16_t glyph =
              embedded_font->glyph_for_code_point(unicode.front());
          glyph != 0) {
        return glyph;
      }
    }
  }
  return static_cast<std::uint16_t>(code); // last resort: code as GID
}

std::string Font::to_unicode(const std::string &codes) const {
  // Encoding fixes the width for simple and Identity fonts (9.10.3, 9.7.5.2).
  if (!cmap.empty()) {
    const std::size_t width = !composite ? 1 : has_identity_encoding() ? 2 : 0;
    return cmap.translate_string(codes, width);
  }
  if (composite) {
    // A predefined Type0 encoding supplies Unicode directly or through its
    // collection’s CID map; even a partial mapping takes precedence.
    if (!cid_encoding_name.empty()) {
      if (std::optional<std::string> unicode =
              translate_predefined_cmap(cid_encoding_name, codes);
          unicode.has_value()) {
        return *unicode;
      }
    }
    // Use CIDSystemInfo only for identity or embedded encoding CMaps.
    // An unsupported named CMap must not be treated as identity.
    const bool identity_cids =
        cid_encoding_name.empty() || has_identity_encoding();
    if (identity_cids && !cid_registry.empty() && !cid_ordering.empty()) {
      std::string result;
      for (const std::uint32_t cid : this->codes(codes)) {
        if (const std::optional<char32_t> unicode =
                cid_to_unicode(cid_registry, cid_ordering, cid);
            unicode.has_value()) {
          util::string::append_c32(*unicode, result);
        }
      }
      if (!result.empty()) {
        return result;
      }
    }
    // The embedded font's reverse map before giving up.
    return reverse_map_unicode(*this, codes);
  }
  if (encoding.has_value()) {
    return encoding->translate_string(codes);
  }
  // No `ToUnicode` CMap and no `/Encoding`: try the embedded reverse map,
  // else keep the historic identity fallback (1-byte code -> code point).
  if (std::string unicode = reverse_map_unicode(*this, codes);
      !unicode.empty()) {
    return unicode;
  }
  return cmap.translate_string(codes, 1);
}

} // namespace odr::internal::pdf
