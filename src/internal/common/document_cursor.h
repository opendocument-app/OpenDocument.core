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

  [[nodiscard]] Element *element() final;
  [[nodiscard]] const Element *element() const final;

  using Allocator = std::function<void *(std::size_t size)>;

  class Element : public abstract::DocumentCursor::Element {
  public:
    bool move_to_parent(abstract::DocumentCursor *cursor) override;
    bool move_to_first_child(abstract::DocumentCursor *cursor) override;
    bool move_to_previous_sibling(abstract::DocumentCursor *cursor) override;
    bool move_to_next_sibling(abstract::DocumentCursor *cursor) override;

    virtual Element *first_child(const DocumentCursor *cursor,
                                 const Allocator &allocator);
    virtual Element *previous_sibling(const DocumentCursor *cursor,
                                      const Allocator &allocator);
    virtual Element *next_sibling(const DocumentCursor *cursor,
                                  const Allocator &allocator);

    class Text : public abstract::DocumentCursor::Element::Text {
    public:
    };

    class Slide : public abstract::DocumentCursor::Element::Slide {
    public:
      [[nodiscard]] bool move_to_master_page(
          abstract::DocumentCursor *cursor,
          const abstract::DocumentCursor::Element *element) const override;

      [[nodiscard]] virtual Element *
      master_page(const DocumentCursor *cursor,
                  const Allocator &allocator) const = 0;
    };

    class Table : public abstract::DocumentCursor::Element::Table {
    public:
      [[nodiscard]] bool move_to_first_table_column(
          abstract::DocumentCursor *cursor,
          const abstract::DocumentCursor::Element *element) const override;
      [[nodiscard]] bool move_to_first_table_row(
          abstract::DocumentCursor *cursor,
          const abstract::DocumentCursor::Element *element) const override;

      [[nodiscard]] virtual Element *
      first_table_column(const DocumentCursor *cursor,
                         const Allocator &allocator) const = 0;
      [[nodiscard]] virtual Element *
      first_table_row(const DocumentCursor *cursor,
                      const Allocator &allocator) const = 0;
    };

    class TableCell : public abstract::DocumentCursor::Element::TableCell {
    public:
    };

    class Image : public abstract::DocumentCursor::Element::Image {
    public:
    };

    [[nodiscard]] const Text *
    text(const abstract::DocumentCursor *cursor) const override;
    [[nodiscard]] const Slide *
    slide(const abstract::DocumentCursor *cursor) const override;
    [[nodiscard]] const Table *
    table(const abstract::DocumentCursor *cursor) const override;
    [[nodiscard]] const TableCell *
    table_cell(const abstract::DocumentCursor *cursor) const override;
    [[nodiscard]] const Image *
    image(const abstract::DocumentCursor *cursor) const override;
  };

protected:
  void *push_(std::size_t size);
  void pop_();
  [[nodiscard]] std::int32_t next_offset_() const;
  [[nodiscard]] std::int32_t back_offset_() const;
  Element *back_();
  [[nodiscard]] const Element *back_() const;

private:
  std::vector<std::int32_t> m_element_stack_top;
  std::string m_element_stack;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_CURSOR_H
