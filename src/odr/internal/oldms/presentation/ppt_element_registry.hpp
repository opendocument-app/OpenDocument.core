#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/style.hpp>

#include <odr/internal/oldms/presentation/ppt_structs.hpp>

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace odr::internal::oldms::presentation {

class ElementRegistry final {
public:
  struct Element final {
    ElementIdentifier parent_id{null_element_id};
    ElementIdentifier first_child_id{null_element_id};
    ElementIdentifier last_child_id{null_element_id};
    ElementIdentifier previous_sibling_id{null_element_id};
    ElementIdentifier next_sibling_id{null_element_id};
    ElementType type{ElementType::none};
  };

  struct Text final {
    std::string text;
  };

  struct Frame final {
    // Position/size in master units; absent for a shape without a ClientAnchor.
    std::optional<Anchor> anchor;
  };

  struct Image final {
    std::string data; //< the raw image file bytes (JPEG/PNG)
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Frame &> create_frame_element();
  std::tuple<ElementIdentifier, Element &, Image &> create_image_element();

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Text &text_element_at(ElementIdentifier id);
  [[nodiscard]] Frame &frame_element_at(ElementIdentifier id);
  [[nodiscard]] Image &image_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Text &text_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Frame &frame_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Image &image_element_at(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);

  /// Stores a font name and returns a pointer that stays valid for the
  /// registry's lifetime (`TextStyle::font_name` is a `const char *`).
  const char *intern_font_name(const std::string &name);

  /// Character style of a span or paragraph element.
  void set_element_style(ElementIdentifier id, TextStyle style);
  [[nodiscard]] const TextStyle *element_style(ElementIdentifier id) const;

  /// Slide dimensions from the DocumentAtom, in master units (1/576 inch).
  void set_slide_size(std::int32_t width, std::int32_t height);
  [[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>>
  slide_size() const;

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Frame> m_frames;
  std::unordered_map<ElementIdentifier, Image> m_images;
  std::unordered_map<ElementIdentifier, TextStyle> m_styles;
  std::optional<std::pair<std::int32_t, std::int32_t>> m_slide_size;
  /// Deque: elements never move, so interned `c_str()`s stay valid.
  std::deque<std::string> m_font_names;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_frame_id(ElementIdentifier id) const;
  void check_image_id(ElementIdentifier id) const;
};

} // namespace odr::internal::oldms::presentation
