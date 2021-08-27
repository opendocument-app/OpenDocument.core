#ifndef ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
#define ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H

#include <functional>
#include <internal/abstract/document.h>
#include <pugixml.hpp>
#include <string>
#include <vector>

namespace odr::internal::common {

class DocumentCursor : public abstract::DocumentCursor,
                       private abstract::SlideElement,
                       private abstract::TableElement,
                       private abstract::ImageElement {
public:
  DocumentCursor();

  bool operator==(const abstract::DocumentCursor &rhs) const final;
  bool operator!=(const abstract::DocumentCursor &rhs) const final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

  [[nodiscard]] std::string document_path() const final;

  [[nodiscard]] ElementType element_type() const final;
  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties() const override;

  abstract::SlideElement *slide() final;
  abstract::TableElement *table() final;
  const abstract::ImageElement *image() const final;

  bool move_to_parent() final;
  bool move_to_first_child() final;
  bool move_to_previous_sibling() final;
  bool move_to_next_sibling() final;

  using Allocator = std::function<void *(std::size_t size)>;

  class Element;

  class SlideElement {
  public:
    virtual ~SlideElement() = default;

    virtual Element *slide_master(const DocumentCursor &cursor,
                                  const Allocator &allocator) = 0;
  };

  class TableElement {
  public:
    virtual ~TableElement() = default;

    [[nodiscard]] virtual bool
    move_to_first_column(const DocumentCursor &cursor) = 0;
    [[nodiscard]] virtual bool
    move_to_first_row(const DocumentCursor &cursor) = 0;
  };

  class ImageElement {
  public:
    virtual ~ImageElement() = default;

    [[nodiscard]] virtual bool internal(const DocumentCursor &cursor) const = 0;
    [[nodiscard]] virtual std::optional<odr::File>
    image_file(const DocumentCursor &cursor) const = 0;
  };

  class Element {
  public:
    virtual ~Element() = default;

    virtual bool operator==(const Element &rhs) const = 0;
    virtual bool operator!=(const Element &rhs) const = 0;

    [[nodiscard]] virtual ElementType type() const = 0;
    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(const DocumentCursor &cursor) const = 0;

    [[nodiscard]] virtual SlideElement *slide(const DocumentCursor &cursor) = 0;
    [[nodiscard]] virtual TableElement *table(const DocumentCursor &cursor) = 0;
    [[nodiscard]] virtual const ImageElement *
    image(const DocumentCursor &cursor) const = 0;

    virtual Element *first_child(const DocumentCursor &cursor,
                                 const Allocator &allocator) = 0;
    virtual Element *previous_sibling(const DocumentCursor &cursor,
                                      const Allocator &allocator) = 0;
    virtual Element *next_sibling(const DocumentCursor &cursor,
                                  const Allocator &allocator) = 0;
  };

protected:
  void *push_(std::size_t size);
  void pop_();
  std::int32_t next_offset_() const;
  std::int32_t back_offset_() const;
  Element *back_();
  const Element *back_() const;

private:
  std::vector<std::int32_t> m_element_stack_top;
  std::string m_element_stack;

  // abstract::SlideElement
  [[nodiscard]] bool move_to_slide_master() final;

  // abstract::TableElement
  [[nodiscard]] bool move_to_first_column() final;
  [[nodiscard]] bool move_to_first_row() final;

  // abstract::ImageElement
  [[nodiscard]] bool internal() const final;
  [[nodiscard]] std::optional<odr::File> image_file() const final;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
