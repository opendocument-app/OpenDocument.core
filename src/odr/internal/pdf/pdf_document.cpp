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
  const long index = static_cast<long>(code) - first_char;
  if (index >= 0 && index < static_cast<long>(widths.size())) {
    return widths[static_cast<std::size_t>(index)] / 1000.0;
  }
  return missing_width / 1000.0;
}

namespace {

/// Embedded-font reverse map: translate `codes` to Unicode via the embedded
/// font's own character map (`code -> glyph -> code_point_for_glyph`). The
/// last resort before byte-identity, closing the extraction gap for
/// fonts with neither a `/ToUnicode` CMap nor a usable `/Encoding`. Returns
/// empty (so the run stays `no_unicode`) when nothing maps or no font is
/// embedded; a partially mapped run yields the code points it could recover.
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
  // Simple TrueType (ISO 32000-1 9.6.6.4), best effort: the embedded cmap keyed
  // on the byte code first (symbolic (3,0)/(1,0) fonts), then on the code's
  // Unicode (via the /Encoding glyph name), then the code as a GID.
  if (const std::uint16_t glyph =
          embedded_font->glyph_for_code_point(static_cast<char32_t>(code));
      glyph != 0) {
    return glyph;
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
  if (!cmap.empty()) {
    return cmap.translate_string(codes);
  }
  if (composite) {
    // A composite (Type0) font with no `ToUnicode` CMap. A predefined Unicode
    // `/Encoding` (the `Uni*-UCS2/UTF16/UTF32` CMaps) carries Unicode directly
    // in its codes, so decode it. Otherwise code -> CID is
    // known (identity for `Identity-H/V`) but CID -> Unicode needs a predefined
    // CID -> Unicode table (the legacy CMaps, deferred) or the embedded font's
    // reverse map (below): emit "no Unicode" rather than
    // mis-splitting the multi-byte codes into byte-sized garbage through the
    // identity fallback below. `extract_text` marks such runs `no_unicode`;
    // their glyphs are still re-encoded to the PUA for display.
    if (!cid_encoding_name.empty()) {
      if (std::optional<std::string> unicode =
              translate_predefined_cmap(cid_encoding_name, codes)) {
        return *unicode;
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
  return cmap.translate_string(codes);
}

} // namespace odr::internal::pdf
