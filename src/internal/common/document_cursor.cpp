#include <cstdint>
#include <internal/abstract/allocator.h>
#include <internal/abstract/document_cursor.h>
#include <internal/abstract/document_element.h>
#include <internal/common/document_cursor.h>
#include <internal/common/document_path.h>
#include <internal/common/style.h>
#include <stdexcept>
#include <variant>

namespace odr::internal::common {

DocumentCursor::DocumentCursor(const abstract::Document *document)
    : m_document{document} {}

DocumentCursor::~DocumentCursor() {
  if (!m_current_element.empty()) {
    DocumentCursor::element()->~Element();
  }
  while (!m_parent_element_stack.empty()) {
    pop_parent_();
  }
}

bool DocumentCursor::equals(const abstract::DocumentCursor &other) const {
  return element()->equals(
      *dynamic_cast<const DocumentCursor &>(other).element());
}

[[nodiscard]] DocumentPath DocumentCursor::document_path() const {
  if (m_current_component) {
    return m_parent_path.join(*m_current_component);
  }
  return m_parent_path;
}

abstract::Element *DocumentCursor::element() {
  return reinterpret_cast<abstract::Element *>(m_current_element.data());
}

const abstract::Element *DocumentCursor::element() const {
  return reinterpret_cast<const abstract::Element *>(m_current_element.data());
}

void *DocumentCursor::reset_current_(const std::size_t size) {
  if (!m_current_element.empty()) {
    element()->~Element();
  }
  m_current_element.resize(size);
  return m_current_element.data();
}

void *DocumentCursor::reset_temporary_(const std::size_t size) {
  if (!m_temporary_element.empty()) {
    temporary_()->~Element();
  }
  m_temporary_element.resize(size);
  return m_temporary_element.data();
}

void *DocumentCursor::push_parent_(const std::size_t size) {
  std::int32_t offset = parent_next_offset_();
  std::int32_t next_offset = offset + size;
  m_parent_element_stack_top.push_back(next_offset);
  m_parent_element_stack.resize(next_offset);
  return m_parent_element_stack.data() + offset;
}

void DocumentCursor::pop_parent_() {
  parent_()->~Element();
  m_parent_element_stack_top.pop_back();
  std::int32_t next_offset = parent_next_offset_();
  m_parent_element_stack.resize(next_offset);
}

void DocumentCursor::push_style_(const ResolvedStyle &style) {
  if (m_style_stack.empty()) {
    m_style_stack.emplace_back();
  } else {
    m_style_stack.emplace_back(m_style_stack.back());
  }
  m_style_stack.back().override(style);
}

void DocumentCursor::pop_style_() { m_style_stack.pop_back(); }

void DocumentCursor::pushed_(abstract::Element *) {
  push_style_(partial_style());
}

void DocumentCursor::popping_(abstract::Element *) { pop_style_(); }

ResolvedStyle DocumentCursor::partial_style() const { return {}; }

abstract::Element *DocumentCursor::temporary_() {
  return reinterpret_cast<abstract::Element *>(m_temporary_element.data());
}

abstract::Element *DocumentCursor::parent_() {
  auto offset = parent_back_offset_();
  return reinterpret_cast<abstract::Element *>(m_parent_element_stack.data() +
                                               offset);
}

std::int32_t DocumentCursor::parent_next_offset_() const {
  return m_parent_element_stack_top.empty()
             ? 0
             : m_parent_element_stack_top[m_parent_element_stack_top.size() -
                                          1];
}

std::int32_t DocumentCursor::parent_back_offset_() const {
  return m_parent_element_stack_top.size() <= 1
             ? 0
             : m_parent_element_stack_top[m_parent_element_stack_top.size() -
                                          2];
}

void DocumentCursor::swap_current_temporary() {
  std::swap(m_current_element, m_temporary_element);
}

bool DocumentCursor::move_to_parent() {
  if (m_parent_element_stack_top.empty()) {
    return false;
  }

  popping_(element());
  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_current_(size);
  };
  parent_()->construct_copy(allocator);
  pop_parent_();

  if (m_parent_path.empty()) {
    m_current_component = {};
  } else {
    m_current_component = m_parent_path.back();
    m_parent_path = m_parent_path.parent();
  }

  return true;
}

bool DocumentCursor::move_to_first_child() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto first_child = element()->construct_first_child(m_document, allocator);
  if (!first_child) {
    return false;
  }
  allocator = [this](const std::size_t size) { return push_parent_(size); };
  element()->construct_copy(allocator);
  swap_current_temporary();
  pushed_(first_child);

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);

  return true;
}

bool DocumentCursor::move_to_previous_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto previous_sibling =
      element()->construct_previous_sibling(m_document, allocator);
  if (!previous_sibling) {
    return false;
  }
  swap_current_temporary();
  popping_(nullptr);
  pushed_(element());

  std::visit([](auto &&c) { --c; }, *m_current_component);

  return true;
}

bool DocumentCursor::move_to_next_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto next_sibling = element()->construct_next_sibling(m_document, allocator);
  if (!next_sibling) {
    return false;
  }
  swap_current_temporary();
  popping_(nullptr);
  pushed_(element());

  std::visit([](auto &&c) { ++c; }, *m_current_component);

  return true;
}

bool DocumentCursor::move_to_master_page() {
  auto slide = dynamic_cast<const abstract::SlideElement *>(element());
  if (!slide) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto master_page = slide->construct_master_page(m_document, allocator);
  if (!master_page) {
    return false;
  }
  allocator = [this](const std::size_t size) { return push_parent_(size); };
  element()->construct_copy(allocator);
  swap_current_temporary();
  pushed_(master_page);

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);

  return true;
}

bool DocumentCursor::move_to_first_table_column() {
  auto table = dynamic_cast<const abstract::TableElement *>(element());
  if (!table) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto first_column = table->construct_first_column(m_document, allocator);
  if (!first_column) {
    return false;
  }
  allocator = [this](const std::size_t size) { return push_parent_(size); };
  element()->construct_copy(allocator);
  swap_current_temporary();
  pushed_(first_column);

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Column(0);

  return true;
}

bool DocumentCursor::move_to_first_table_row() {
  auto table = dynamic_cast<const abstract::TableElement *>(element());
  if (!table) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto first_row = table->construct_first_row(m_document, allocator);
  if (!first_row) {
    return false;
  }
  allocator = [this](const std::size_t size) { return push_parent_(size); };
  element()->construct_copy(allocator);
  swap_current_temporary();
  pushed_(first_row);

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Row(0);

  return true;
}

bool DocumentCursor::move_to_first_sheet_shape() {
  auto sheet = dynamic_cast<const abstract::SheetElement *>(element());
  if (!sheet) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return reset_temporary_(size);
  };
  auto first_shape = sheet->construct_first_shape(m_document, allocator);
  if (!first_shape) {
    return false;
  }
  allocator = [this](const std::size_t size) { return push_parent_(size); };
  element()->construct_copy(allocator);
  swap_current_temporary();
  pushed_(first_shape);

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);

  return true;
}

void DocumentCursor::move(const common::DocumentPath &path) {
  std::uint32_t death = 0;

  try {
    for (auto &&c : path) {
      std::uint32_t number;
      if (auto child = std::get_if<common::DocumentPath::Child>(&c)) {
        if (!move_to_first_child()) {
          throw std::invalid_argument("child not found");
        }
        number = child->number;
      } else if (auto column = std::get_if<common::DocumentPath::Column>(&c)) {
        if (!move_to_first_table_column()) {
          throw std::invalid_argument("column not found");
        }
        number = column->number;
      } else if (auto row = std::get_if<common::DocumentPath::Row>(&c)) {
        if (!move_to_first_table_row()) {
          throw std::invalid_argument("row not found");
        }
        number = row->number;
      } else {
        throw std::invalid_argument("unknown component");
      }
      ++death;
      for (std::uint32_t i = 0; i < number; ++i) {
        if (!move_to_next_sibling()) {
          throw std::invalid_argument("sibling not found");
        }
      }
    }
  } catch (...) {
    for (std::uint32_t i = 0; i < death; ++i) {
      move_to_parent();
    }
    throw;
  }
}

const ResolvedStyle &DocumentCursor::intermediate_style() const {
  return m_style_stack.back();
}

} // namespace odr::internal::common
