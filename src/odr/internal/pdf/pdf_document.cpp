#include <odr/internal/pdf/pdf_document.hpp>

#include <odr/internal/pdf/pdf_cid.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>

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

std::string Font::to_unicode(const std::string &codes) const {
  if (!cmap.empty()) {
    return cmap.translate_string(codes);
  }
  if (composite) {
    // A composite (Type0) font with no `ToUnicode` CMap. A predefined Unicode
    // `/Encoding` (the `Uni*-UCS2/UTF16/UTF32` CMaps) carries Unicode directly
    // in its codes, so decode it (stage 1.3 part B). Otherwise code -> CID is
    // known (identity for `Identity-H/V`) but CID -> Unicode needs a predefined
    // CID -> Unicode table (the legacy CMaps, deferred) or the embedded font
    // program (stage 1.4): emit "no Unicode" rather than mis-splitting the
    // multi-byte codes into byte-sized garbage through the identity fallback
    // below. Stage 1.5 will mark these runs for re-encoding.
    if (!cid_encoding_name.empty()) {
      if (std::optional<std::string> unicode =
              translate_predefined_cmap(cid_encoding_name, codes)) {
        return *unicode;
      }
    }
    return {};
  }
  if (encoding.has_value()) {
    return encoding->translate_string(codes);
  }
  // Neither a `ToUnicode` CMap nor an `/Encoding`: keep the historic identity
  // fallback (single-byte code -> code point).
  return cmap.translate_string(codes);
}

} // namespace odr::internal::pdf
