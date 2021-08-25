#ifndef ODR_INTERNAL_ODF_CURSOR_H
#define ODR_INTERNAL_ODF_CURSOR_H

#include <functional>
#include <internal/abstract/document.h>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::odf {
class OpenDocument;

class DocumentCursor final : public abstract::DocumentCursor {
public:
  DocumentCursor(const OpenDocument *document, pugi::xml_node root);

  bool operator==(const abstract::DocumentCursor &rhs) const final;
  bool operator!=(const abstract::DocumentCursor &rhs) const final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

  [[nodiscard]] std::string document_path() const final;

  [[nodiscard]] ElementType type() const final;

  bool parent() final;
  bool first_child() final;
  bool previous_sibling() final;
  bool next_sibling() final;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final;

  using Allocator = std::function<void *(std::size_t size)>;

  class Element {
  public:
    virtual ~Element() = default;

    virtual bool operator==(const Element &rhs) const = 0;
    virtual bool operator!=(const Element &rhs) const = 0;

    [[nodiscard]] virtual ElementType type() const = 0;

    virtual Element *first_child(Allocator allocator) const = 0;
    virtual Element *previous_sibling(Allocator allocator) const = 0;
    virtual Element *next_sibling(Allocator allocator) const = 0;

    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(const OpenDocument *document) const = 0;
  };

private:
  const OpenDocument *m_document;

  std::vector<std::int32_t> m_element_offset;
  std::string m_element_stack;

  void *push_(std::size_t size);
  void pop_();
  std::int32_t back_offset_() const;
  const Element *back_() const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_CURSOR_H
