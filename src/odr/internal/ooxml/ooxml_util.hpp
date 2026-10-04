#pragma once

#include <odr/style.hpp>

#include <odr/internal/xml/xml_tree_edit.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace pugi {
class xml_attribute;
class xml_document;
class xml_node;
} // namespace pugi

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {
class AbsPath;
} // namespace odr::internal

namespace odr::internal::ooxml {

/// Writes before @p before, or appends: Word text/tab nodes or one DrawingML
/// text node. Empty text still creates a node.
xml::NodeSpan write_text_nodes(pugi::xml_node parent, pugi::xml_node before,
                               const std::string &text,
                               std::string_view prefix);

/// `RRGGBB`, as `w:color/@w:val` and `a:srgbClr/@val` spell one.
std::string hex_color(const Color &color);
/// The `w:highlight` name of @p color, where it is one of the sixteen
/// ([ECMA-376] 17.18.40).
std::optional<std::string_view> highlight_name(const Color &color);
/// @p length in points; refuses a unit that has no fixed size.
double points(const Measure &length);

std::optional<std::string> read_string_attribute(pugi::xml_attribute);
std::optional<Color> read_color_attribute(pugi::xml_attribute);
/// The `a:srgbClr` of @p parent, else the value its `a:sysClr` last resolved
/// to. [ECMA-376] 20.1.2.3.32, 20.1.2.3.33
std::optional<Color> read_drawing_rgb_color(pugi::xml_node parent);
/// [ECMA-376] 18.8.19: a negative @p tint moves the lightness toward black, a
/// positive one toward white.
Color apply_tint(const Color &color, double tint);
std::optional<Measure> read_half_point_attribute(pugi::xml_attribute);
std::optional<Measure> read_eighth_point_attribute(pugi::xml_attribute);
std::optional<Measure> read_hundredth_point_attribute(pugi::xml_attribute);
std::optional<Measure> read_emus_attribute(pugi::xml_attribute);
/// EMUs written as a node's text, the way `wp:posOffset` states an offset.
std::optional<Measure> read_emus_text(pugi::xml_node);
std::optional<Measure> read_twips_attribute(pugi::xml_attribute);
std::optional<Measure> read_pct_attribute(pugi::xml_attribute);
std::optional<Measure> read_width_attribute(pugi::xml_node);
bool read_on_off_attribute(pugi::xml_attribute);
bool read_on_off_attribute(pugi::xml_node);
std::optional<bool> read_line_attribute(pugi::xml_attribute);
std::optional<bool> read_line_attribute(pugi::xml_node);
std::optional<std::string> read_shadow_attribute(pugi::xml_attribute);
std::optional<std::string> read_shadow_attribute(pugi::xml_node);
std::optional<FontWeight> read_font_weight_attribute(pugi::xml_attribute);
std::optional<FontWeight> read_font_weight_attribute(pugi::xml_node);
std::optional<FontStyle> read_font_style_attribute(pugi::xml_attribute);
std::optional<FontStyle> read_font_style_attribute(pugi::xml_node);
std::optional<TextAlign> read_text_align_attribute(pugi::xml_attribute);
std::optional<TextAlign> read_drawing_text_align_attribute(pugi::xml_attribute);
/// [ECMA-376] 17.3.1.6 `w:bidi`; absent says nothing.
std::optional<TextDirection> read_text_direction_attribute(pugi::xml_node);
/// [ECMA-376] 21.1.2.2.7 `a:pPr/@rtl`.
std::optional<TextDirection>
    read_drawing_text_direction_attribute(pugi::xml_attribute);
std::optional<VerticalAlign> read_vertical_align_attribute(pugi::xml_attribute);
std::optional<VerticalAlign>
    read_drawing_vertical_align_attribute(pugi::xml_attribute);
/// [ECMA-376] 17.3.4 `CT_Border`; `w:sz` is in eighths of a point. `nil` and
/// `none` draw nothing, which is not the same as saying nothing.
std::optional<std::string> read_border_node(pugi::xml_node);
/// The four sides of a `w:tblBorders`/`w:tcBorders`.
DirectionalStyle<std::string> read_borders_node(pugi::xml_node);

/// Resolves a package target against its source part; rejects an empty target.
AbsPath resolve_part_path(const AbsPath &source, std::string_view target);

using Relations = std::unordered_map<std::string, std::string>;
using XmlDocumentsAndRelations =
    std::unordered_map<AbsPath, std::pair<pugi::xml_document, Relations>>;
using SharedStrings = std::vector<pugi::xml_node>;

std::unordered_map<std::string, std::string>
parse_relationships(const pugi::xml_document &relations);
std::unordered_map<std::string, std::string>
parse_relationships(const abstract::ReadableFilesystem &filesystem,
                    const AbsPath &path);
std::optional<AbsPath>
parse_relationship_target(const abstract::ReadableFilesystem &filesystem,
                          const AbsPath &path, std::string_view type);
/// Every part a relationship of @p type leads to from @p path, in the order
/// the relationships state them.
std::vector<AbsPath>
parse_relationship_targets(const abstract::ReadableFilesystem &filesystem,
                           const AbsPath &path, std::string_view type);

} // namespace odr::internal::ooxml
