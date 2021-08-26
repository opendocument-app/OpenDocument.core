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

  abstract::ImageElement *image() final;

  bool move_to_parent() final;
  bool move_to_first_child() final;
  bool move_to_previous_sibling() final;
  bool move_to_next_sibling() final;

  using Allocator = std::function<void *(std::size_t size)>;

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

    virtual ImageElement *image(const DocumentCursor &cursor) = 0;

    virtual Element *first_child(const DocumentCursor &cursor,
                                 Allocator allocator) = 0;
    virtual Element *previous_sibling(const DocumentCursor &cursor,
                                      Allocator allocator) = 0;
    virtual Element *next_sibling(const DocumentCursor &cursor,
                                  Allocator allocator) = 0;
  };

protected:
  std::vector<std::int32_t> m_element_offset;
  std::int32_t m_next_element_offset{0};
  std::string m_element_stack;

  void *push_(std::size_t size);
  void pop_();
  std::int32_t back_offset_() const;
  const Element *back_() const;
  Element *back_();

private:
  class ImageElementImpl final : public abstract::ImageElement {
  public:
    ImageElementImpl();
    ImageElementImpl(DocumentCursor *cursor,
                     DocumentCursor::ImageElement *impl);

    [[nodiscard]] bool internal() const final;
    [[nodiscard]] std::optional<odr::File> image_file() const final;

  private:
    DocumentCursor *m_cursor{nullptr};
    DocumentCursor::ImageElement *m_impl{nullptr};
  };

  ImageElementImpl m_image_element;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
