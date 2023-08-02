#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/internal/abstract/document.h>
#include <odr/internal/abstract/document_cursor.h>
#include <odr/internal/abstract/document_element.h>
#include <odr/internal/common/document_path.h>

namespace odr {

DocumentCursor::DocumentCursor(
    std::shared_ptr<internal::abstract::Document> document,
    std::unique_ptr<internal::abstract::DocumentCursor> cursor)
    : m_document{std::move(document)}, m_cursor{std::move(cursor)} {
  // TODO throw if nullptr
}

DocumentCursor::DocumentCursor(const DocumentCursor &other)
    : DocumentCursor(other.m_document, other.m_cursor->copy()) {}

DocumentCursor::~DocumentCursor() = default;

DocumentCursor &DocumentCursor::operator=(const DocumentCursor &other) {
  if (&other == this) {
    return *this;
  }

  m_document = other.m_document;
  m_cursor = other.m_cursor->copy();
  return *this;
}

bool DocumentCursor::operator==(const DocumentCursor &rhs) const {
  return m_cursor->equals(*rhs.m_cursor);
}

bool DocumentCursor::operator!=(const DocumentCursor &rhs) const {
  return !operator==(rhs);
}

std::string DocumentCursor::document_path() const {
  return m_cursor->document_path().to_string();
}

ElementType DocumentCursor::element_type() const {
  return m_cursor->element()->type(m_document.get());
}

bool DocumentCursor::move_to_parent() { return m_cursor->move_to_parent(); }

bool DocumentCursor::move_to_first_child() {
  return m_cursor->move_to_first_child();
}

bool DocumentCursor::move_to_previous_sibling() {
  return m_cursor->move_to_previous_sibling();
}

bool DocumentCursor::move_to_next_sibling() {
  return m_cursor->move_to_next_sibling();
}

Element DocumentCursor::element() const {
  return {m_document.get(), m_cursor.get(), m_cursor->element()};
}

bool DocumentCursor::move_to_master_page() {
  return m_cursor->move_to_master_page();
}

bool DocumentCursor::move_to_first_table_column() {
  return m_cursor->move_to_first_table_column();
}

bool DocumentCursor::move_to_first_table_row() {
  return m_cursor->move_to_first_table_row();
}

bool DocumentCursor::move_to_first_sheet_shape() {
  return m_cursor->move_to_first_sheet_shape();
}

void DocumentCursor::move(const std::string &path) {
  m_cursor->move(internal::common::DocumentPath(path));
}

void DocumentCursor::for_each_child(const ChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_column(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_column()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_row(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_row()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_table_cell(
    const ConditionalChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_sheet_shape(const ChildVisitor &visitor) {
  if (!move_to_first_sheet_shape()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_(const ChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    visitor(*this, i);
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

void DocumentCursor::for_each_(const ConditionalChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    if (!visitor(*this, i)) {
      break;
    }
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

} // namespace odr