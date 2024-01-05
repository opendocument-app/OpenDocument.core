#ifndef ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP
#define ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <unordered_map>
#include <vector>

namespace odr::internal::pdf {

struct Pages;
struct Page;
struct Annotation;
struct Resources;
struct Font;

enum class Type {
  unknown,
  catalog,
  pages,
  page,
  annotation,
  resources,
  font,
};

struct Element {
  virtual ~Element() = default;

  Type type{Type::unknown};
  ObjectReference object_reference;
  Object object;
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

  Resources *resources;
  std::vector<Annotation *> annotations;

  // TODO remove
  ObjectReference contents_reference;
};

struct Annotation : Element {};

struct Resources : Element {
  std::unordered_map<std::string, Font *> font;
};

struct Font : Element {};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_DOCUMENT_ELEMENT_HPP
