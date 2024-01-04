#ifndef ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <vector>

namespace odr::internal::pdf {

struct Pages;
struct Page;

enum class Type {
  unknown,
  catalog,
  pages,
  page,
};

struct Element {
  virtual ~Element() = default;

  Type type{Type::unknown};
  ObjectReference object_reference;
};

struct Catalog : Element {
  Pages *pages{nullptr};
};

struct Pages : Element {
  std::vector<Element *> kids;
  std::uint32_t count{0};
};

struct Page : Element {
  Pages *parent{nullptr};

  // TODO remove
  ObjectReference contents_reference;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP
