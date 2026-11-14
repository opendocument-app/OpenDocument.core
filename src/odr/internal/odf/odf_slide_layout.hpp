#pragma once

#include <odr/slide_layout.hpp>

#include <optional>
#include <string>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal {
struct BlankSlideLayout;
} // namespace odr::internal

namespace odr::internal::odf {

/// A layout a slide can start from, and where it comes from: a
/// `style:presentation-page-layout` of the file, or a built-in it lacks.
struct SlideLayoutSource final {
  SlideLayout layout;
  pugi::xml_node node; ///< null for a built-in the file does not state
  const BlankSlideLayout *builtin{nullptr};
};

/// The `presentation:placeholder` @p node as a text placeholder; none for
/// another object or one without a position.
[[nodiscard]] std::optional<SlidePlaceholder>
read_text_placeholder(pugi::xml_node node);

/// The layouts of @p styles (`office:styles`), named by `style:display-name`,
/// else after the placeholders they hold, else by `style:name`, a repeat
/// numbered; then each built-in whose name none of them has, on a page of
/// @p width by @p height centimetres. A layout for handouts or notes is left
/// out.
[[nodiscard]] std::vector<SlideLayoutSource>
list_slide_layouts(pugi::xml_node styles, double width, double height);

/// Writes @p layout into @p styles, scaled from the built-in page onto a
/// page of @p width by @p height centimetres.
pugi::xml_node write_slide_layout(pugi::xml_node styles,
                                  const BlankSlideLayout &layout, double width,
                                  double height);

} // namespace odr::internal::odf
