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
  DocumentCursor();

  bool operator==(const abstract::DocumentCursor &rhs) const final;
  bool operator!=(const abstract::DocumentCursor &rhs) const final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final;

  [[nodiscard]] std::string document_path() const final;

  [[nodiscard]] ElementType element_type() const final;
  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties() const override;

  bool move_to_parent() final;
  bool move_to_first_child() final;
  bool move_to_previous_sibling() final;
  bool move_to_next_sibling() final;

  [[nodiscard]] std::string text() const final;

  [[nodiscard]] bool move_to_master_page() final;

  [[nodiscard]] TableDimensions table_dimensions() const final;
  [[nodiscard]] bool move_to_first_table_column() final;
  [[nodiscard]] bool move_to_first_table_row() final;
  [[nodiscard]] TableDimensions table_cell_span() const final;

  [[nodiscard]] bool image_internal() const final;
  [[nodiscard]] std::optional<odr::File> image_file() const final;

  using Allocator = std::function<void *(std::size_t size)>;

  class Element {
  public:
    virtual ~Element() = default;

    [[nodiscard]] virtual bool equals(const DocumentCursor &cursor,
                                      const Element &rhs) const = 0;

    [[nodiscard]] virtual ElementType
    type(const DocumentCursor &cursor) const = 0;
    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(const DocumentCursor &cursor) const = 0;

    virtual Element *first_child(const DocumentCursor &cursor,
                                 const Allocator &allocator) = 0;
    virtual Element *previous_sibling(const DocumentCursor &cursor,
                                      const Allocator &allocator) = 0;
    virtual Element *next_sibling(const DocumentCursor &cursor,
                                  const Allocator &allocator) = 0;

    [[nodiscard]] virtual std::string
    text(const DocumentCursor &cursor) const = 0;

    virtual Element *master_page(const DocumentCursor &cursor,
                                 const Allocator &allocator) = 0;

    [[nodiscard]] virtual TableDimensions
    table_dimensions(const DocumentCursor &cursor) const = 0;
    virtual Element *first_table_column(const DocumentCursor &cursor,
                                        const Allocator &allocator) = 0;
    virtual Element *first_table_row(const DocumentCursor &cursor,
                                     const Allocator &allocator) = 0;
    [[nodiscard]] virtual TableDimensions
    table_cell_span(const DocumentCursor &cursor) const = 0;

    [[nodiscard]] virtual bool
    image_internal(const DocumentCursor &cursor) const = 0;
    [[nodiscard]] virtual std::optional<odr::File>
    image_file(const DocumentCursor &cursor) const = 0;
  };

  class DefaultElement : public Element {
  public:
    [[nodiscard]] std::unordered_map<ElementProperty, std::any>
    properties(const DocumentCursor &cursor) const override;

    Element *first_child(const DocumentCursor &cursor,
                         const Allocator &allocator) override;
    Element *previous_sibling(const DocumentCursor &cursor,
                              const Allocator &allocator) override;
    Element *next_sibling(const DocumentCursor &cursor,
                          const Allocator &allocator) override;

    [[nodiscard]] std::string text(const DocumentCursor &cursor) const override;

    Element *master_page(const DocumentCursor &cursor,
                         const Allocator &allocator) override;

    [[nodiscard]] TableDimensions
    table_dimensions(const DocumentCursor &cursor) const override;
    Element *first_table_column(const DocumentCursor &cursor,
                                const Allocator &allocator) override;
    Element *first_table_row(const DocumentCursor &cursor,
                             const Allocator &allocator) override;
    [[nodiscard]] TableDimensions
    table_cell_span(const DocumentCursor &cursor) const override;

    [[nodiscard]] bool
    image_internal(const DocumentCursor &cursor) const override;
    [[nodiscard]] std::optional<odr::File>
    image_file(const DocumentCursor &cursor) const override;
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
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
