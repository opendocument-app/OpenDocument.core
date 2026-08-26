#pragma once

#include <odr/style.hpp>

#include <optional>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {
class AbsPath;
} // namespace odr::internal

namespace odr::internal::ooxml::presentation {

/// The theme's `a:clrScheme` seen through the slide master's `p:clrMap`, so a
/// slide's `a:schemeClr` names a colour. [ECMA-376] 20.1.6.2
class ColorScheme final {
public:
  ColorScheme() = default;
  ColorScheme(pugi::xml_node color_scheme, pugi::xml_node color_map);

  [[nodiscard]] std::optional<Color> resolve(const char *name) const;

private:
  std::unordered_map<std::string, Color> m_colors;
};

/// What a slide layout, and the master behind it, decide for every slide that
/// hangs off them.
struct LayoutStyle final {
  const ColorScheme *color_scheme{nullptr};
  std::optional<Color> background;
};

/// A drawingml colour choice: a literal, a theme slot, or a system colour.
/// [ECMA-376] 20.1.2.3
std::optional<Color> read_drawing_color(pugi::xml_node parent,
                                        const ColorScheme *color_scheme);

/// Whether the part states a ground at all, and the colour where it states one
/// we model: a `p:bgRef`, gradient or image ends the inheritance walk rather
/// than falling through to the part behind it. [ECMA-376] 19.3.1.1
std::optional<std::optional<Color>>
read_background_color(pugi::xml_node slide_like,
                      const ColorScheme *color_scheme);

/// The scheme of the master behind @p layout_path, cached in @p color_schemes
/// by master path, and the ground the layout states, else its master's. A part
/// that is missing or is not xml leaves the slide unstyled rather than failing
/// the open.
LayoutStyle
load_layout_style(const abstract::ReadableFilesystem &filesystem,
                  const AbsPath &layout_path,
                  std::unordered_map<std::string, ColorScheme> &color_schemes);

void resolve_text_style(pugi::xml_node node, const ColorScheme *color_scheme,
                        TextStyle &result);
void resolve_paragraph_style(pugi::xml_node node, ParagraphStyle &result);

} // namespace odr::internal::ooxml::presentation
