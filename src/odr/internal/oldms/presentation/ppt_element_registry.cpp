#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>

namespace odr::internal::oldms::presentation {

std::tuple<ElementIdentifier, ElementRegistry::Element &>
ElementRegistry::create_element(const ElementType type) {
  return create_element_(type);
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Text &>
ElementRegistry::create_text_element() {
  const auto &[element_id, element] = create_element_(ElementType::text);
  Text &text = m_texts.emplace(element_id, Text{});
  return {element_id, element, text};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Frame &>
ElementRegistry::create_frame_element() {
  const auto &[element_id, element] = create_element_(ElementType::frame);
  Frame &frame = m_frames.emplace(element_id, Frame{});
  return {element_id, element, frame};
}

std::tuple<ElementIdentifier, ElementRegistry::Element &,
           ElementRegistry::Image &>
ElementRegistry::create_image_element() {
  const auto &[element_id, element] = create_element_(ElementType::image);
  Image &image = m_images.emplace(element_id, Image{});
  return {element_id, element, image};
}

void ElementRegistry::set_slide_size(const std::int32_t width,
                                     const std::int32_t height) {
  m_slide_size = {width, height};
}

std::optional<std::pair<std::int32_t, std::int32_t>>
ElementRegistry::slide_size() const {
  return m_slide_size;
}

void ElementRegistry::set_element_style_index(const ElementIdentifier id,
                                              const std::uint32_t index) {
  check_element_id(id);
  m_style_indices.emplace(id, index);
}

std::uint32_t
ElementRegistry::element_style_index(const ElementIdentifier id) const {
  const std::uint32_t *index = m_style_indices.find(id);
  return index != nullptr ? *index : 0;
}

} // namespace odr::internal::oldms::presentation
