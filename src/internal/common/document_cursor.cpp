#include <internal/common/document_cursor.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::common {

DocumentCursor::DocumentCursor() { m_element_stack_top.push_back(0); }

bool DocumentCursor::operator==(const abstract::DocumentCursor &rhs) const {
  return back_()->equals(this,
                         *dynamic_cast<const DocumentCursor &>(rhs).back_());
}

bool DocumentCursor::operator!=(const abstract::DocumentCursor &rhs) const {
  return !operator==(rhs);
}

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

[[nodiscard]] std::string DocumentCursor::document_path() const {
  return ""; // TODO
}

abstract::DocumentCursor::Element *DocumentCursor::element() { return back_(); }

const abstract::DocumentCursor::Element *DocumentCursor::element() const {
  return back_();
}

void *DocumentCursor::push_(const std::size_t size) {
  std::int32_t offset = next_offset_();
  std::int32_t next_offset = offset + size;
  m_element_stack_top.push_back(next_offset);
  m_element_stack.resize(next_offset);
  return m_element_stack.data() + offset;
}

void DocumentCursor::pop_() {
  back_()->~Element();
  m_element_stack_top.pop_back();
  std::int32_t next_offset = next_offset_();
  m_element_stack.resize(next_offset);
}

std::int32_t DocumentCursor::next_offset_() const {
  return m_element_stack_top.back();
}

std::int32_t DocumentCursor::back_offset_() const {
  return (m_element_stack_top.size() <= 1)
             ? 0
             : m_element_stack_top[m_element_stack_top.size() - 2];
}

DocumentCursor::Element *DocumentCursor::back_() {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<Element *>(m_element_stack.data() + offset);
}

const DocumentCursor::Element *DocumentCursor::back_() const {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<const Element *>(m_element_stack.data() + offset);
}

bool DocumentCursor::Element::move_to_parent(
    abstract::DocumentCursor *abstract_cursor) {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  if (cursor->m_element_stack_top.size() <= 1) {
    return false;
  }
  cursor->pop_();
  return true;
}

bool DocumentCursor::Element::move_to_first_child(
    abstract::DocumentCursor *abstract_cursor) {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    return cursor->push_(size);
  };
  auto element = first_child(cursor, allocator);
  return element != nullptr;
}

bool DocumentCursor::Element::move_to_previous_sibling(
    abstract::DocumentCursor *abstract_cursor) {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    cursor->pop_();
    return cursor->push_(size);
  };
  auto element = previous_sibling(cursor, allocator);
  return element != nullptr;
}

bool DocumentCursor::Element::move_to_next_sibling(
    abstract::DocumentCursor *abstract_cursor) {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    cursor->pop_();
    return cursor->push_(size);
  };
  auto element = next_sibling(cursor, allocator);
  return element != nullptr;
}

const DocumentCursor::Element::Text *
DocumentCursor::Element::text(const abstract::DocumentCursor *) const {
  return nullptr;
}

const DocumentCursor::Element::Slide *
DocumentCursor::Element::slide(const abstract::DocumentCursor *) const {
  return nullptr;
}

const DocumentCursor::Element::Table *
DocumentCursor::Element::table(const abstract::DocumentCursor *) const {
  return nullptr;
}

const DocumentCursor::Element::TableCell *
DocumentCursor::Element::table_cell(const abstract::DocumentCursor *) const {
  return nullptr;
}

const DocumentCursor::Element::Image *
DocumentCursor::Element::image(const abstract::DocumentCursor *) const {
  return nullptr;
}

DocumentCursor::Element *
DocumentCursor::Element::first_child(const DocumentCursor *,
                                     const Allocator &) {
  return nullptr;
}

DocumentCursor::Element *
DocumentCursor::Element::previous_sibling(const DocumentCursor *,
                                          const Allocator &) {
  return nullptr;
}

DocumentCursor::Element *
DocumentCursor::Element::next_sibling(const DocumentCursor *,
                                      const Allocator &) {
  return nullptr;
}

bool DocumentCursor::Element::Slide::move_to_master_page(
    abstract::DocumentCursor *abstract_cursor,
    const abstract::DocumentCursor::Element *) const {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    return cursor->push_(size);
  };
  auto element = master_page(cursor, allocator);
  return element != nullptr;
}

bool DocumentCursor::Element::Table::move_to_first_table_column(
    abstract::DocumentCursor *abstract_cursor,
    const abstract::DocumentCursor::Element *) const {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    return cursor->push_(size);
  };
  auto element = first_table_column(cursor, allocator);
  return element != nullptr;
}

bool DocumentCursor::Element::Table::move_to_first_table_row(
    abstract::DocumentCursor *abstract_cursor,
    const abstract::DocumentCursor::Element *) const {
  auto *cursor = dynamic_cast<DocumentCursor *>(abstract_cursor);
  auto allocator = [cursor](const std::size_t size) {
    return cursor->push_(size);
  };
  auto element = first_table_row(cursor, allocator);
  return element != nullptr;
}

} // namespace odr::internal::common
