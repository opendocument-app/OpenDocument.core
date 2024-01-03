#ifndef ODR_INTERNAL_PDF_DOCUMENT_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_HPP

#include <memory>
#include <vector>

namespace odr::internal::pdf {

struct Catalog;
struct Element;

struct Document {
  Catalog *catalog;
  std::vector<std::unique_ptr<Element>> element;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_HPP
