#include <odr/internal/pdf/pdf_document.hpp>

#include <odr/internal/pdf/pdf_document_element.hpp>

#include <stdexcept>

namespace odr::internal::pdf {

namespace {

template <typename Collection>
void collect_pages_impl(const Pages &pages, Collection &out) {
  for (Element *kid : pages.kids) {
    if (auto *inner = dynamic_cast<Pages *>(kid); inner != nullptr) {
      collect_pages_impl(*inner, out);
    } else if (auto *page = dynamic_cast<Page *>(kid); page != nullptr) {
      out.push_back(page);
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
