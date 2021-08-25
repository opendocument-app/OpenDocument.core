#include <internal/odf/odf_cursor.h>
#include <odr/element.h>

namespace odr::internal::odf {

namespace {
template <typename Derived, ElementType element_type>
class DefaultElement : public DocumentCursor::Element {
public:
  pugi::xml_node m_node;

  explicit DefaultElement(pugi::xml_node node) : m_node{node} {}

  bool operator==(const Element &rhs) const override {
    return m_node == *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  bool operator!=(const Element &rhs) const override {
    return m_node != *dynamic_cast<const DefaultElement &>(rhs).m_node;
  }

  ElementType type() const final { return element_type; }
};

class TextDocumentRoot final
    : public DefaultElement<TextDocumentRoot, ElementType::ROOT> {
public:
  Element *first_child(DocumentCursor::Allocator allocator) const final {
    return nullptr; // TODO
  }

  Element *previous_sibling(DocumentCursor::Allocator allocator) const final {
    return nullptr;
  }

  Element *next_sibling(DocumentCursor::Allocator allocator) const final {
    return nullptr;
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties(const OpenDocument *document) const final {
    return {}; // TODO
  }
};
} // namespace

DocumentCursor::DocumentCursor(const OpenDocument *document,
                               pugi::xml_node root)
    : m_document{document} {
  // TODO init with root
}

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

[[nodiscard]] ElementType DocumentCursor::type() const {
  return back_()->type();
}

bool DocumentCursor::parent() {
  if (m_element_offset.size() <= 1) {
    return false;
  }
  pop_();
  return true;
}

bool DocumentCursor::first_child() {
  auto allocator = [this](std::size_t size) { return push_(size); };
  auto element = back_()->first_child(allocator);
  return element != nullptr;
}

bool DocumentCursor::previous_sibling() {
  auto allocator = [this](std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->previous_sibling(allocator);
  return element != nullptr;
}

bool DocumentCursor::next_sibling() {
  auto allocator = [this](std::size_t size) {
    pop_();
    return push_(size);
  };
  auto element = back_()->next_sibling(allocator);
  return element != nullptr;
}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
DocumentCursor::properties() const {
  return back_()->properties(m_document);
}

void *DocumentCursor::push_(const std::size_t size) {
  std::int32_t offset = back_offset_();
  m_element_offset.push_back(offset);
  m_element_stack.reserve(offset + size);
  return m_element_stack.data() + offset;
}

void DocumentCursor::pop_() {
  back_()->~Element();
  std::int32_t offset = back_offset_();
  m_element_offset.pop_back();
  m_element_stack.reserve(offset);
}

std::int32_t DocumentCursor::back_offset_() const {
  return m_element_offset.empty() ? 0 : m_element_offset.back();
}

const DocumentCursor::Element *DocumentCursor::back_() const {
  std::int32_t offset = back_offset_();
  return reinterpret_cast<const Element *>(m_element_offset.data() + offset);
}

} // namespace odr::internal::odf
