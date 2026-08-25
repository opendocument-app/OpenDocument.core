#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace odr::internal::iwork {

class ElementRegistry final {
public:
  struct Size final {
    float width{};
    float height{};
  };

  /// A drawable's rectangle, in points. A side of the extent is absent for a
  /// box that grows with its text rather than one of zero extent.
  struct Rect final {
    float x{};
    float y{};
    std::optional<float> width{};
    std::optional<float> height{};
  };

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

  /// Where a drawable sits on its page, in points. Absent for one whose
  /// geometry we did not find.
  struct Frame final {
    std::optional<Rect> rect;
  };

  /// A slide's name and the size of the page it is shown on, in points.
  struct Slide final {
    std::string name;
    std::optional<Size> size;
  };

  void clear() noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

  std::tuple<ElementIdentifier, Element &> create_element(ElementType type);
  std::tuple<ElementIdentifier, Element &, Text &> create_text_element();
  std::tuple<ElementIdentifier, Element &, Frame &> create_frame_element();
  std::tuple<ElementIdentifier, Element &, Slide &> create_slide_element();

  [[nodiscard]] Element &element_at(ElementIdentifier id);
  [[nodiscard]] Text &text_element_at(ElementIdentifier id);
  [[nodiscard]] Frame &frame_element_at(ElementIdentifier id);
  [[nodiscard]] Slide &slide_element_at(ElementIdentifier id);

  [[nodiscard]] const Element &element_at(ElementIdentifier id) const;
  [[nodiscard]] const Text &text_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Frame &frame_element_at(ElementIdentifier id) const;
  [[nodiscard]] const Slide &slide_element_at(ElementIdentifier id) const;

  void append_child(ElementIdentifier parent_id, ElementIdentifier child_id);

private:
  std::vector<Element> m_elements;
  std::unordered_map<ElementIdentifier, Text> m_texts;
  std::unordered_map<ElementIdentifier, Frame> m_frames;
  std::unordered_map<ElementIdentifier, Slide> m_slides;

  void check_element_id(ElementIdentifier id) const;
  void check_text_id(ElementIdentifier id) const;
  void check_frame_id(ElementIdentifier id) const;
  void check_slide_id(ElementIdentifier id) const;
};

} // namespace odr::internal::iwork
