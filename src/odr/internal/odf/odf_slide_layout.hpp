#pragma once

#include <odr/quantity.hpp>

#include <string>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal {
struct BlankSlideLayout;
} // namespace odr::internal

namespace odr::internal::odf {

/// A layout a slide can start from: a `style:presentation-page-layout` of
/// the file, or a built-in it lacks.
struct SlideLayoutSource final {
  std::string name;
  pugi::xml_node node; ///< null for a built-in the file does not state
  const BlankSlideLayout *builtin{nullptr};
};

/// The layouts of @p styles (`office:styles`), named by `style:display-name`,
/// else after the placeholders they hold, else by `style:name`, a repeat
/// numbered; then each built-in whose name none of them has. A layout for
/// handouts or notes is left out.
[[nodiscard]] std::vector<SlideLayoutSource>
list_slide_layouts(pugi::xml_node styles);

/// Writes @p layout into @p styles, scaled from the built-in page onto a
/// page of @p width by @p height centimetres.
pugi::xml_node write_slide_layout(pugi::xml_node styles,
                                  const BlankSlideLayout &layout, double width,
                                  double height);

/// Fills @p page as a slide named @p name on @p master with a frame and an
/// empty paragraph per text placeholder of @p layout, styled by the master's
/// presentation styles in @p styles.
void write_slide(pugi::xml_node page, const std::string &name,
                 const std::string &master, pugi::xml_node layout,
                 pugi::xml_node styles);

/// Fills @p frame as a text box of graphic style @p style holding one empty
/// paragraph, at @p x and @p y and @p width by @p height.
void write_text_box(pugi::xml_node frame, const std::string &style,
                    const Measure &x, const Measure &y, const Measure &width,
                    const Measure &height);

} // namespace odr::internal::odf
