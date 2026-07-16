#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>

#include <algorithm>
#include <stdexcept>

namespace odr::internal::oldms::presentation {

void ElementRegistry::clear() noexcept {
  m_elements.clear();
  m_texts.clear();
  m_frames.clear();
  m_images.clear();
  m_styles.clear();
  m_font_names.clear();
  m_slide_size.reset();
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
           ElementRegistry::Image &>
ElementRegistry::create_image_element() {
  const auto &[element_id, element] = create_element(ElementType::image);
  auto [it, success] = m_images.emplace(element_id, Image{});
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

ElementRegistry::Image &
ElementRegistry::image_element_at(const ElementIdentifier id) {
  check_image_id(id);
  return m_images.at(id);
}

const ElementRegistry::Image &
ElementRegistry::image_element_at(const ElementIdentifier id) const {
  check_image_id(id);
  return m_images.at(id);
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

void ElementRegistry::set_slide_size(const std::int32_t width,
                                     const std::int32_t height) {
  m_slide_size = {width, height};
}

std::optional<std::pair<std::int32_t, std::int32_t>>
ElementRegistry::slide_size() const {
  return m_slide_size;
}

const char *ElementRegistry::intern_font_name(const std::string &name) {
  if (const auto it = std::ranges::find(m_font_names, name);
      it != m_font_names.end()) {
    return it->c_str();
  }
  return m_font_names.emplace_back(name).c_str();
}

void ElementRegistry::set_element_style(const ElementIdentifier id,
                                        TextStyle style) {
  check_element_id(id);
  m_styles[id] = std::move(style);
}

const TextStyle *
ElementRegistry::element_style(const ElementIdentifier id) const {
  const auto it = m_styles.find(id);
  return it != m_styles.end() ? &it->second : nullptr;
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

void ElementRegistry::check_image_id(const ElementIdentifier id) const {
  check_element_id(id);
  if (!m_images.contains(id)) {
    throw std::out_of_range(
        "ElementRegistry::check_id: image identifier not found");
  }
}

} // namespace odr::internal::oldms::presentation
