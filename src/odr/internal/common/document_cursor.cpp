#include <cstdint>
#include <odr/internal/abstract/document_cursor.hpp>
#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/common/document_cursor.hpp>
#include <odr/internal/common/document_path.hpp>
#include <odr/internal/common/style.hpp>
#include <stdexcept>
#include <variant>

namespace odr::internal::common {

DocumentCursor::DocumentCursor(const abstract::Document *document)
    : m_document{document} {}

DocumentCursor::DocumentCursor(const DocumentCursor &other)
    : m_document{other.m_document}, m_style_stack{other.m_style_stack},
      m_parent_path{other.m_parent_path}, m_current_component{
                                              other.m_current_component} {
  for (auto &&element : other.m_element_stack) {
    m_element_stack.push_back(element->construct_copy());
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
  if (m_element_stack.empty()) {
    return nullptr;
  }
  return m_element_stack.back().get();
}

const abstract::Element *DocumentCursor::element() const {
  if (m_element_stack.empty()) {
    return nullptr;
  }
  return m_element_stack.back().get();
}

void DocumentCursor::push_element_(std::unique_ptr<abstract::Element> element) {
  m_element_stack.push_back(std::move(element));
  pushed_(this->element());
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

bool DocumentCursor::move_to_parent() {
  if (m_element_stack.empty()) {
    return false;
  }

  popping_(element());
  m_element_stack.pop_back();

  if (m_parent_path.empty()) {
    m_current_component = {};
  } else {
    m_current_component = m_parent_path.back();
    m_parent_path = m_parent_path.parent();
  }

  return true;
}

bool DocumentCursor::move_to_first_child() {
  auto first_child = element()->construct_first_child(m_document);
  if (!first_child) {
    return false;
  }
  m_element_stack.push_back(std::move(first_child));
  pushed_(element());

  if (m_current_component) {
    m_parent_path = m_parent_path.join(*m_current_component);
  }
  m_current_component = DocumentPath::Child(0);

  return true;
}

bool DocumentCursor::move_to_previous_sibling() {
  auto previous_sibling = element()->construct_previous_sibling(m_document);
  if (!previous_sibling) {
    return false;
  }
  popping_(nullptr);
  m_element_stack.pop_back();
  m_element_stack.push_back(std::move(previous_sibling));
  pushed_(element());

  std::visit([](auto &&c) { --c; }, *m_current_component);

  return true;
}

bool DocumentCursor::move_to_next_sibling() {
  auto next_sibling = element()->construct_next_sibling(m_document);
  if (!next_sibling) {
    return false;
  }
  popping_(nullptr);
  m_element_stack.pop_back();
  m_element_stack.push_back(std::move(next_sibling));
  pushed_(element());

  std::visit([](auto &&c) { ++c; }, *m_current_component);

  return true;
}

bool DocumentCursor::move_to_master_page() {
  auto slide = dynamic_cast<const abstract::SlideElement *>(element());
  if (!slide) {
    return false;
  }

  auto master_page = slide->construct_master_page(m_document);
  if (!master_page) {
    return false;
  }
  m_element_stack.push_back(std::move(master_page));
  pushed_(element());

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

  auto first_column = table->construct_first_column(m_document);
  if (!first_column) {
    return false;
  }
  m_element_stack.push_back(std::move(first_column));
  pushed_(element());

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

  auto first_row = table->construct_first_row(m_document);
  if (!first_row) {
    return false;
  }
  m_element_stack.push_back(std::move(first_row));
  pushed_(element());

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

  auto first_shape = sheet->construct_first_shape(m_document);
  if (!first_shape) {
    return false;
  }
  m_element_stack.push_back(std::move(first_shape));
  pushed_(element());

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
