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

} // namespace odr::internal::pdf
