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

/// A master's theme: its colours, and the latin typefaces `+mj-lt` and
/// `+mn-lt` name. [ECMA-376] 20.1.4.1.24 and 20.1.4.1.25
struct Theme final {
  ColorScheme color_scheme;
  std::string major_font;
  std::string minor_font;
};

/// What a slide layout, and the master behind it, decide for every slide that
/// hangs off them.
struct LayoutStyle final {
  const Theme *theme{nullptr};
  std::optional<Color> background;
  pugi::xml_node layout; ///< `p:sldLayout`, null where it did not parse
  pugi::xml_node master; ///< `p:sldMaster`, null where it did not parse
};

/// Where a slide shape's text and geometry come from when it does not state
/// them. [ECMA-376] 19.3.1.36
struct PlaceholderSources final {
  pugi::xml_node layout_shape;
  pugi::xml_node master_shape;
  /// `p:titleStyle` or `p:bodyStyle` of the master's `p:txStyles`
  pugi::xml_node master_list_style;
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

/// The layouts, masters and themes of one package, each read once. A
/// returned style points into this, so it has to outlive the style.
class LayoutStyles final {
public:
  /// The theme of the master behind @p layout_path, and the ground the layout
  /// states, else its master's. A part that is missing or is not xml leaves
  /// the slide unstyled rather than failing the open.
  const LayoutStyle &get(const abstract::ReadableFilesystem &files,
                         const AbsPath &layout_path);

private:
  /// by path; `unordered_map` keeps its elements put, so pointers into these
  /// survive later inserts
  std::unordered_map<std::string, pugi::xml_document> m_parts;
  std::unordered_map<std::string, Theme> m_themes;
  std::unordered_map<std::string, LayoutStyle> m_layouts;

  pugi::xml_node part(const abstract::ReadableFilesystem &files,
                      const AbsPath &path);
  const Theme &theme(const abstract::ReadableFilesystem &files,
                     const AbsPath &master_path, pugi::xml_node master);
};

/// The placeholders of @p layout and @p master that @p shape, a `p:sp`,
/// inherits from: by `idx`, else by `type`. A shape that is no placeholder
/// inherits from none.
PlaceholderSources find_placeholder_sources(pugi::xml_node shape,
                                            pugi::xml_node layout,
                                            pugi::xml_node master);

/// Reads an `a:rPr` or an `a:defRPr` onto @p result.
void resolve_text_properties(pugi::xml_node properties, const Theme *theme,
                             TextStyle &result);
/// Reads an `a:pPr` or a list style level such as `a:lvl1pPr` onto
/// @p result.
void resolve_paragraph_properties(pugi::xml_node properties,
                                  ParagraphStyle &result);

} // namespace odr::internal::ooxml::presentation
