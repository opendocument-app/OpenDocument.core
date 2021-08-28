#include <internal/common/document_cursor.h>
#include <odr/file.h>

namespace odr::internal::common {

DocumentCursor::DocumentCursor() { m_element_stack_top.push_back(0); }

bool DocumentCursor::operator==(const abstract::DocumentCursor &rhs) const {
  return back_()->operator==(
      *dynamic_cast<const DocumentCursor &>(rhs).back_());
}

bool DocumentCursor::operator!=(const abstract::DocumentCursor &rhs) const {
  return back_()->operator!=(
      *dynamic_cast<const DocumentCursor &>(rhs).back_());
}

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

[[nodiscard]] std::string DocumentCursor::document_path() const {
  return ""; // TODO
}

[[nodiscard]] ElementType DocumentCursor::element_type() const {
  return back_()->type();
}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
DocumentCursor::element_properties() const {
  return back_()->properties(*this);
}

bool DocumentCursor::move_to_parent() {
  if (m_element_stack_top.size() <= 1) {
    return false;
  }
  pop_();
  return true;
}

bool DocumentCursor::move_to_first_child() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  auto element = back_()->first_child(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_previous_sibling() {
  auto allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->previous_sibling(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_next_sibling() {
  auto allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  auto impl = back_();
  auto element = impl->next_sibling(*this, allocator);
  return element != nullptr;
}

std::string DocumentCursor::text() const { return back_()->text(*this); }

bool DocumentCursor::move_to_slide_master() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  auto element = back_()->slide_master(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_first_table_column() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  auto element = back_()->first_table_column(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_first_table_row() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  auto element = back_()->first_table_row(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::image_internal() const {
  return back_()->image_internal(*this);
}

std::optional<odr::File> DocumentCursor::image_file() const {
  return back_()->image_file(*this);
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
} // namespace odr::internal::common
