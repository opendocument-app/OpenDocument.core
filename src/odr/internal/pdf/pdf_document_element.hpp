#pragma once

#include <odr/internal/pdf/pdf_cmap.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <concepts>
#include <optional>
#include <unordered_map>
#include <vector>

namespace odr::internal::pdf {

struct Pages;
struct Page;
struct Annotation;
struct Resources;
struct Font;

struct Element {
  virtual ~Element() = default;

  ObjectReference object_reference;
  Object object;

  /// Value-semantic discrimination over the element hierarchy, backed by RTTI;
  /// mirrors `Object`'s `is_*`/`as_*` surface. `T` must be a complete-derived
  /// type at the call site.
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] bool is() const {
    return dynamic_cast<const T *>(this) != nullptr;
  }
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] T &as() {
    return dynamic_cast<T &>(*this);
  }
  template <typename T>
    requires std::derived_from<T, Element>
  [[nodiscard]] const T &as() const {
    return dynamic_cast<const T &>(*this);
  }
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
  /// `ToUnicode` CMap, the primary code -> Unicode path when present.
  CMap cmap;
  /// Simple-font `/Encoding` (base + `/Differences`), the text-extraction
  /// fallback used when no `ToUnicode` CMap is present.
  std::optional<Encoding> encoding;

  /// Translate a string of character codes to Unicode: the `ToUnicode` CMap
  /// when present (authoritative), else the `/Encoding`, else identity bytes.
  [[nodiscard]] std::string to_unicode(const std::string &codes) const;
};

} // namespace odr::internal::pdf
