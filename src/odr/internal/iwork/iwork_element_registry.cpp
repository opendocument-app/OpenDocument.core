#include <odr/internal/iwork/iwork_element_registry.hpp>

#include <stdexcept>

namespace odr::internal::iwork {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_frames.clear();
  m_slides.clear();
}

[[nodiscard]] std::size_t ElementRegistry::size() const noexcept {
  return m_elements.size();
}

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  Element &element = m_elements.emplace_back();
  ElementIdentifier element_id = m_elements.size();
  element.type = type;
  return {element_id, element};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element(ElementType::text);
  auto [it, success] = m_texts.emplace(element_id, Text{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Frame &>
ElementRegistry::create_frame_element() {
  const auto &[element_id, element] = create_element(ElementType::frame);
  auto [it, success] = m_frames.emplace(element_id, Frame{});
  return {element_id, element, it->second};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Slide &>
ElementRegistry::create_slide_element() {
  const auto &[element_id, element] = create_element(ElementType::slide);
  auto [it, success] = m_slides.emplace(element_id, Slide{});
  return {element_id, element, it->second};
}

ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) {
  check_element_id(id);
  return m_elements.at(id - 1);
}

ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) {
  check_text_id(id);
  return m_texts.at(id);
}

ElementRegistry::Frame &
ElementRegistry::frame_element_at(const ElementIdentifier id) {
  check_frame_id(id);
  return m_frames.at(id);
}

ElementRegistry::Slide &
ElementRegistry::slide_element_at(const ElementIdentifier id) {
  check_slide_id(id);
  return m_slides.at(id);
}

const ElementRegistry::Element &
ElementRegistry::element_at(const ElementIdentifier id) const {
  check_element_id(id);
  return m_elements.at(id - 1);
}

const ElementRegistry::Text &
ElementRegistry::text_element_at(const ElementIdentifier id) const {
  check_text_id(id);
  return m_texts.at(id);
}

const ElementRegistry::Frame &
ElementRegistry::frame_element_at(const ElementIdentifier id) const {
  check_frame_id(id);
  return m_frames.at(id);
}

const ElementRegistry::Slide &
ElementRegistry::slide_element_at(const ElementIdentifier id) const {
  check_slide_id(id);
  return m_slides.at(id);
}

void ElementRegistry::append_child(const ElementIdentifier parent_id,
                                   const ElementIdentifier child_id) {
  check_element_id(parent_id);
  check_element_id(child_id);
  if (element_at(child_id).parent_id != null_element_id) {
    throw std::invalid_argument(
        "ElementRegistry::append_child: child already has a parent");
  }

  const ElementIdentifier previous_sibling_id =
      element_at(parent_id).last_child_id;

  element_at(child_id).parent_id = parent_id;
  element_at(child_id).previous_sibling_id = previous_sibling_id;

  if (element_at(parent_id).first_child_id == null_element_id) {
    element_at(parent_id).first_child_id = child_id;
  } else {
    element_at(previous_sibling_id).next_sibling_id = child_id;
  }
  element_at(parent_id).last_child_id = child_id;
}

void ElementRegistry::check_element_id(const ElementIdentifier id) const {
  if (id == null_element_id) {
    throw std::out_of_range("ElementRegistry::check_id: null identifier");
  }
  if (id - 1 >= m_elements.size()) {
    throw std::out_of_range(
        "ElementRegistry::check_id: identifier out of range");
  }
}

void ElementRegistry::check_text_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_texts.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_frame_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_frames.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

void ElementRegistry::check_slide_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_slides.contains(id)) {
    throw std::out_of_range("ElementRegistry::check_id: identifier not found");
  }
}

} // namespace odr::internal::iwork
