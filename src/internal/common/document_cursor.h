#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <functional>
#include <internal/abstract/document.h>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::common {

class DocumentCursor : public abstract::DocumentCursor {
public:
  bool operator==(const abstract::DocumentCursor &rhs) const final;
  bool operator!=(const abstract::DocumentCursor &rhs) const final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

  [[nodiscard]] std::string document_path() const final;

  [[nodiscard]] ElementType element_type() const final;
  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties() const override;

  abstract::TableElement *table() const final;
  abstract::ImageElement *image() const final;

  bool move_to_parent() final;
  bool move_to_first_child() final;
  bool move_to_previous_sibling() final;
  bool move_to_next_sibling() final;

  using Allocator = std::function<void *(std::size_t size)>;

  class Element {
  public:
    virtual ~Element() = default;

    virtual bool operator==(const Element &rhs) const = 0;
    virtual bool operator!=(const Element &rhs) const = 0;

    [[nodiscard]] virtual ElementType type() const = 0;
    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(const DocumentCursor &cursor) const = 0;

    virtual abstract::TableElement *table() const = 0;
    virtual abstract::ImageElement *image() const = 0;

    virtual Element *first_child(const DocumentCursor &cursor,
                                 Allocator allocator) const = 0;
    virtual Element *previous_sibling(const DocumentCursor &cursor,
                                      Allocator allocator) const = 0;
    virtual Element *next_sibling(const DocumentCursor &cursor,
                                  Allocator allocator) const = 0;
  };

protected:
  std::vector<std::int32_t> m_element_offset;
  std::string m_element_stack;

  void *push_(std::size_t size);
  void pop_();
  std::int32_t back_offset_() const;
  const Element *back_() const;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
