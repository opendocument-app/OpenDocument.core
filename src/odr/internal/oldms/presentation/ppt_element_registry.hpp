#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/common/element_registry.hpp>
#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace odr::internal::oldms::presentation {

class ElementRegistry final
    : public internal::ElementRegistry<ElementNode<ElementIdentifier>> {
public:
  struct Text final {
    std::string text;
  };

  struct Frame final {
    // Position/size in master units; absent for a shape without a ClientAnchor.
    std::optional<Anchor> anchor;
  };

  struct Image final {
    std::string data; //< the raw image file bytes (JPEG/PNG)
    std::string href; //< pseudo-path naming the BLIP (no real container path)
  };

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Frame &> create_frame_element();
  std::tuple<ElementIdentifier, Element &, Image &> create_image_element();

  [[nodiscard]] Text &text_element_at(const ElementIdentifier id) {
    return m_texts.at(id);
  }
  [[nodiscard]] Frame &frame_element_at(const ElementIdentifier id) {
    return m_frames.at(id);
  }
  [[nodiscard]] Image &image_element_at(const ElementIdentifier id) {
    return m_images.at(id);
  }

  [[nodiscard]] const Text &text_element_at(const ElementIdentifier id) const {
    return m_texts.at(id);
  }
  [[nodiscard]] const Frame &
  frame_element_at(const ElementIdentifier id) const {
    return m_frames.at(id);
  }
  [[nodiscard]] const Image &
  image_element_at(const ElementIdentifier id) const {
    return m_images.at(id);
  }

  /// Character style of a span or paragraph element, as an index into the
  /// document's `StyleRegistry` (0 is the default style).
  void set_element_style_index(ElementIdentifier id, std::uint32_t index);
  [[nodiscard]] std::uint32_t element_style_index(ElementIdentifier id) const;

  /// Slide dimensions from the DocumentAtom, in master units (1/576 inch).
  void set_slide_size(std::int32_t width, std::int32_t height);
  [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
  slide_size() const;

private:
  SideTable<Text> m_texts;
  SideTable<Frame> m_frames;
  SideTable<Image> m_images;
  SideTable<std::uint32_t> m_style_indices;
  std::optional<std::pair<std::int32_t, std::int32_t>> m_slide_size;
};

} // namespace odr::internal::oldms::presentation
