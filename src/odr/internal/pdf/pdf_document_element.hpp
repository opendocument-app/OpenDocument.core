#pragma once

#include <odr/internal/pdf/pdf_cmap.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <stdexcept>
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

struct Catalog final : Element {
  Pages *pages{nullptr};
};

struct Pages final : Element {
  std::vector<Element *> kids;
  std::uint32_t count{0};
};

struct Page final : Element {
  Pages *parent{nullptr};

  Resources *resources{nullptr};
  std::vector<Annotation *> annotations;

  // resolved inheritable attributes (ISO 32000-1 7.7.3.3, Table 30)
  Object media_box;  // rectangle array
  Object crop_box;   // rectangle array (defaults to media_box)
  Integer rotate{0}; // normalized to {0, 90, 180, 270}

  // TODO remove
  std::vector<ObjectReference> contents_reference;
};

struct Annotation final : Element {};

struct Resources final : Element {
  std::unordered_map<std::string, Font *> font;
};

struct Font final : Element {
  CMap cmap;
};

} // namespace odr::internal::pdf
