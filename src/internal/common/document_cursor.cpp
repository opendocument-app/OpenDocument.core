#include <internal/common/document_cursor.h>
#include <odr/document.h>
#include <odr/file.h>

namespace odr::internal::common {

DocumentCursor::DocumentCursor(const abstract::Document *document)
    : m_document{document} {
  m_element_stack_top.push_back(0);
}

bool DocumentCursor::equals(const abstract::DocumentCursor &other) const {
  return back_()->equals(m_document, this,
                         *dynamic_cast<const DocumentCursor &>(other).back_());
}

[[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
DocumentCursor::copy() const {
  return std::make_unique<DocumentCursor>(*this);
}

[[nodiscard]] std::string DocumentCursor::document_path() const {
  return ""; // TODO
}

abstract::Element *DocumentCursor::element() { return back_(); }

const abstract::Element *DocumentCursor::element() const { return back_(); }

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

abstract::Element *DocumentCursor::back_() {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<abstract::Element *>(m_element_stack.data() + offset);
}

const abstract::Element *DocumentCursor::back_() const {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<const abstract::Element *>(m_element_stack.data() +
                                                     offset);
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
  return back_()->first_child(
      m_document, this,
      reinterpret_cast<const abstract::Allocator *>(&allocator));
}

bool DocumentCursor::move_to_previous_sibling() {
  auto allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  return back_()->previous_sibling(
      m_document, this,
      reinterpret_cast<const abstract::Allocator *>(&allocator));
}

bool DocumentCursor::move_to_next_sibling() {
  auto allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  return back_()->next_sibling(
      m_document, this,
      reinterpret_cast<const abstract::Allocator *>(&allocator));
}

bool DocumentCursor::move_to_master_page() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  if (auto slide = dynamic_cast<const abstract::SlideElement *>(back_())) {
    return slide->master_page(
        m_document, this,
        reinterpret_cast<const abstract::Allocator *>(&allocator));
  }
  return false;
}

bool DocumentCursor::move_to_first_table_column() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  if (auto table = dynamic_cast<const abstract::TableElement *>(back_())) {
    return table->first_column(
        m_document, this,
        reinterpret_cast<const abstract::Allocator *>(&allocator));
  }
  return false;
}

bool DocumentCursor::move_to_first_table_row() {
  auto allocator = [this](const std::size_t size) { return push_(size); };
  if (auto table = dynamic_cast<const abstract::TableElement *>(back_())) {
    return table->first_row(
        m_document, this,
        reinterpret_cast<const abstract::Allocator *>(&allocator));
  }
  return false;
}

} // namespace odr::internal::common
