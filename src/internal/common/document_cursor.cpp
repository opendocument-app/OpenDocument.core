#include <internal/common/document_cursor.h>
#include <odr/file.h>

namespace odr::internal::common {

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

abstract::ImageElement *DocumentCursor::image() {
  auto impl = back_()->image(*this);
  if (!impl) {
    return nullptr;
  }
  m_image_element = ImageElementImpl(this, impl);
  return &m_image_element;
}

bool DocumentCursor::move_to_parent() {
  if (m_element_offset.size() <= 1) {
    return false;
  }
  pop_();
  return true;
}

bool DocumentCursor::move_to_first_child() {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = back_()->first_child(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_previous_sibling() {
  auto allocator = [this](std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->previous_sibling(*this, allocator);
  return element != nullptr;
}

bool DocumentCursor::move_to_next_sibling() {
  auto allocator = [this](std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->next_sibling(*this, allocator);
  return element != nullptr;
}

void *DocumentCursor::push_(const std::size_t size) {
  m_element_offset.push_back(m_next_element_offset);
  std::int32_t offset = back_offset_();
  m_next_element_offset = offset + size;
  m_element_stack.reserve(m_next_element_offset);
  return m_element_stack.data() + offset;
}

void DocumentCursor::pop_() {
  back_()->~Element();
  std::int32_t offset = back_offset_();
  m_element_offset.pop_back();
  m_element_stack.reserve(offset);
}

std::int32_t DocumentCursor::back_offset_() const {
  return m_element_offset.empty() ? m_next_element_offset
                                  : m_element_offset.back();
}

const DocumentCursor::Element *DocumentCursor::back_() const {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<const Element *>(m_element_stack.data() + offset);
}

DocumentCursor::Element *DocumentCursor::back_() {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<Element *>(m_element_stack.data() + offset);
}

DocumentCursor::ImageElementImpl::ImageElementImpl() = default;

DocumentCursor::ImageElementImpl::ImageElementImpl(
    DocumentCursor *cursor, DocumentCursor::ImageElement *impl)
    : m_cursor{cursor}, m_impl{impl} {}

bool DocumentCursor::ImageElementImpl::internal() const {
  return m_impl->internal(*m_cursor);
}

std::optional<odr::File> DocumentCursor::ImageElementImpl::image_file() const {
  return m_impl->image_file(*m_cursor);
}

} // namespace odr::internal::common
