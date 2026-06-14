#include <odr/internal/pdf/pdf_document.hpp>

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
  if (encoding.has_value()) {
    return encoding->translate_string(codes);
  }
  // Neither a `ToUnicode` CMap nor an `/Encoding`: keep the historic identity
  // fallback (single-byte code -> code point).
  return cmap.translate_string(codes);
}

} // namespace odr::internal::pdf
