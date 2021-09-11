#include <internal/common/document_cursor.h>
#include <iostream>
#include <odr/file.h>

namespace odr::internal::common {

DocumentCursor::DocumentCursor(const abstract::Document *document)
    : m_document{document} {
  m_element_stack_top.push_back(0);
}

DocumentCursor::~DocumentCursor() {
  while (!m_element_stack.empty()) {
    pop_();
  }
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

void DocumentCursor::pushed_(abstract::Element *) {
  if (m_style_stack.empty()) {
    m_style_stack.emplace_back();
  } else {
    m_style_stack.emplace_back(m_style_stack.back());
  }
  m_style_stack.back().override(partial_style());
}

void DocumentCursor::popping_(abstract::Element *) { m_style_stack.pop_back(); }

ResolvedStyle DocumentCursor::partial_style() const { return {}; }

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

bool DocumentCursor::move_helper_(abstract::Element *element) {
  if (element) {
    pushed_(element);
  }
  return element;
}

bool DocumentCursor::move_to_parent() {
  if (m_element_stack_top.size() <= 1) {
    return false;
  }
  popping_(back_());
  pop_();
  return true;
}

bool DocumentCursor::move_to_first_child() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  return move_helper_(back_()->first_child(m_document, this, &allocator));
}

bool DocumentCursor::move_to_previous_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    popping_(back_());
    pop_();
    return push_(size);
  };
  return move_helper_(back_()->previous_sibling(m_document, this, &allocator));
}

bool DocumentCursor::move_to_next_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    popping_(back_());
    pop_();
    return push_(size);
  };
  return move_helper_(back_()->next_sibling(m_document, this, &allocator));
}

bool DocumentCursor::move_to_master_page() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  if (auto slide = dynamic_cast<const abstract::SlideElement *>(back_())) {
    return move_helper_(slide->master_page(m_document, this, &allocator));
  }
  return false;
}

bool DocumentCursor::move_to_first_table_column() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  if (auto table = dynamic_cast<const abstract::TableElement *>(back_())) {
    return move_helper_(table->first_column(m_document, this, &allocator));
  }
  return false;
}

bool DocumentCursor::move_to_first_table_row() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  if (auto table = dynamic_cast<const abstract::TableElement *>(back_())) {
    return move_helper_(table->first_row(m_document, this, &allocator));
  }
  return false;
}

const ResolvedStyle &DocumentCursor::intermediate_style() const {
  return m_style_stack.back();
}

} // namespace odr::internal::common
