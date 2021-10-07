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

[[nodiscard]] DocumentPath DocumentCursor::document_path() const {
  if (m_current_component) {
    return m_parent_path.join(*m_current_component);
  }
  return m_parent_path;
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
  popping_(back_());
  pop_();
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
    return push_(size);
  };
  auto element = back_()->first_child(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  pushed_(element);
  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);
  return true;
}

bool DocumentCursor::move_to_previous_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->previous_sibling(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  popping_(element);
  pushed_(element);
  std::visit([](auto &&c) { --c; }, *m_current_component);
  return true;
}

bool DocumentCursor::move_to_next_sibling() {
  abstract::Allocator allocator = [this](const std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->next_sibling(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  popping_(element);
  pushed_(element);
  std::visit([](auto &&c) { ++c; }, *m_current_component);
  return true;
}

bool DocumentCursor::move_to_master_page() {
  auto slide = dynamic_cast<const abstract::SlideElement *>(back_());
  if (!slide) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  auto element = slide->master_page(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  pushed_(element);
  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);
  return true;
}

bool DocumentCursor::move_to_first_table_column() {
  auto table = dynamic_cast<const abstract::TableElement *>(back_());
  if (!table) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  auto element = table->first_column(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  pushed_(element);
  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Column(0);
  return true;
}

bool DocumentCursor::move_to_first_table_row() {
  auto table = dynamic_cast<const abstract::TableElement *>(back_());
  if (!table) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  auto element = table->first_row(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  pushed_(element);
  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Row(0);
  return true;
}

bool DocumentCursor::move_to_first_sheet_shape() {
  auto sheet = dynamic_cast<const abstract::SheetElement *>(back_());
  if (!sheet) {
    return false;
  }

  abstract::Allocator allocator = [this](const std::size_t size) {
    return push_(size);
  };
  auto element = sheet->first_shape(m_document, this, &allocator);
  if (!element) {
    return false;
  }
  pushed_(element);
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
