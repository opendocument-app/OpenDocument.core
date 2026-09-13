#include <odr/internal/ooxml/ooxml_util.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace odr::internal {

namespace {

// value readers shared by the `w:val`-node and the bare-attribute overloads

bool line_from_value(const char *value) {
  return std::strcmp("none", value) != 0 && std::strcmp("false", value) != 0 &&
         std::strcmp("noStrike", value) != 0;
}

std::optional<FontWeight> font_weight_from_value(const char *value) {
  if (std::strcmp("false", value) == 0 || std::strcmp("0", value) == 0) {
    return FontWeight::normal;
  }
  return FontWeight::bold;
}

std::optional<FontStyle> font_style_from_value(const char *value) {
  if (std::strcmp("false", value) == 0) {
    return {};
  }
  return FontStyle::italic;
}

} // namespace

xml::NodeSpan ooxml::write_text_nodes(pugi::xml_node parent,
                                      const pugi::xml_node before,
                                      const std::string &text,
                                      const std::string_view prefix) {
  xml::NodeSpan span;
  const std::string text_tag = std::string(prefix) + ":t";
  const std::string tab_tag = std::string(prefix) + ":tab";

  const auto insert = [&](const std::string &name) {
    const pugi::xml_node node = before
                                    ? parent.insert_child_before(name, before)
                                    : parent.append_child(name);
    if (!span.first) {
      span.first = node;
    }
    span.last = node;
    return node;
  };
  // [ECMA-376] Part 1 17.3.3.31: without `xml:space` a reader collapses the
  // space at either end of a `w:t`, and a lone space is part of a `string`
  // token - so the text says whether one is there, not the token type.
  const auto insert_text = [&](const std::string &token) {
    pugi::xml_node node = insert(text_tag);
    if (token.starts_with(' ') || token.ends_with(' ')) {
      node.append_attribute("xml:space").set_value("preserve");
    }
    node.append_child(pugi::xml_node_type::node_pcdata)
        .text()
        .set(token.c_str());
  };

  for (const xml::StringToken &token : xml::tokenize_text(text)) {
    switch (token.type) {
    case xml::StringToken::Type::none:
      break;
    case xml::StringToken::Type::string:
    case xml::StringToken::Type::spaces:
      insert_text(token.string);
      break;
    case xml::StringToken::Type::tabs:
      for (std::size_t i = 0; i < token.string.size(); ++i) {
        insert(tab_tag);
      }
      break;
    }
  }

  if (!span.first) {
    insert(text_tag);
  }
  return span;
}

std::optional<std::string>
ooxml::read_string_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return attribute.value();
}

namespace {

/// [ECMA-376] 17.18.40 `ST_HighlightColor`, less `none`. The rgb values are
/// what Word paints for each name.
constexpr std::array<std::pair<std::string_view, std::uint32_t>, 16>
    highlight_colors{{
        {"black", 0x000000},
        {"blue", 0x0070c0},
        {"cyan", 0x00b0f0},
        {"darkBlue", 0x002060},
        {"darkCyan", 0x006185},
        {"darkGray", 0x404040},
        {"darkGreen", 0x008000},
        {"darkMagenta", 0x7030a0},
        {"darkRed", 0xc00000},
        {"darkYellow", 0x806000},
        {"green", 0x00b050},
        {"lightGray", 0xbfbfbf},
        {"magenta", 0xff00ff},
        {"red", 0xff0000},
        {"white", 0xffffff},
        {"yellow", 0xffff00},
    }};

} // namespace

pugi::xml_node
ooxml::insert_in_sequence(pugi::xml_node parent, const char *name,
                          const std::span<const std::string_view> order) {
  const auto rank = [&](const std::string_view child_name) {
    const auto it = std::ranges::find(order, child_name);
    return it == std::end(order)
               ? order.size()
               : static_cast<std::size_t>(it - std::begin(order));
  };
  const std::size_t own_rank = rank(name);
  for (const pugi::xml_node child : parent.children()) {
    if (rank(child.name()) > own_rank) {
      return parent.insert_child_before(name, child);
    }
  }
  return parent.append_child(name);
}

std::string ooxml::hex_color(const Color &color) {
  return fmt::format("{:06X}", color.rgb());
}

std::optional<std::string_view> ooxml::highlight_name(const Color &color) {
  const auto it =
      std::ranges::find(highlight_colors, color.rgb(),
                        &std::pair<std::string_view, std::uint32_t>::second);
  if (it == std::end(highlight_colors)) {
    return {};
  }
  return it->first;
}

double ooxml::points(const Measure &length) {
  const std::string &unit = length.unit().name();
  if (unit == "pt") {
    return length.magnitude();
  }
  if (unit == "px") {
    return length.magnitude() * 0.75;
  }
  if (unit == "in") {
    return length.magnitude() * 72.0;
  }
  if (unit == "cm") {
    return length.magnitude() * 72.0 / 2.54;
  }
  if (unit == "mm") {
    return length.magnitude() * 72.0 / 25.4;
  }
  if (unit == "pc") {
    return length.magnitude() * 12.0;
  }
  throw std::invalid_argument("no fixed size in points: " + length.to_string());
}

std::optional<Color>
ooxml::read_color_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("auto", value) == 0 || std::strcmp("none", value) == 0) {
    return {};
  }
  if (const auto it =
          std::ranges::find(highlight_colors, std::string_view(value),
                            &std::pair<std::string_view, std::uint32_t>::first);
      it != std::end(highlight_colors)) {
    return Color::from_rgb(it->second);
  }
  if (std::strlen(value) == 6) {
    const std::uint32_t color = std::strtoull(value, nullptr, 16);
    return Color::from_rgb(color);
  }
  return {};
}

std::optional<Measure>
ooxml::read_half_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() * 0.5, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_eighth_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() * 0.125, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_hundredth_point_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() * 0.01, DynamicUnit("pt"));
}

std::optional<Measure>
ooxml::read_emus_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() / 914400.0, DynamicUnit("in"));
}

std::optional<Measure> ooxml::read_emus_text(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return Measure(node.text().as_double() / 914400.0, DynamicUnit("in"));
}

std::optional<Measure>
ooxml::read_twips_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return Measure(attribute.as_double() / 1440.0, DynamicUnit("in"));
}

std::optional<Measure>
ooxml::read_pct_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }

  // a table width in "pct" is in fiftieths of a percent unless it carries a
  // literal `%` — http://officeopenxml.com/WPtableWidth.php
  std::string val = attribute.value();
  util::string::trim_inplace(val);

  if (val.find('%') != std::string::npos) {
    util::string::replace_all(val, "%", "");
    return Measure(std::stod(val), DynamicUnit("%"));
  }

  return Measure(attribute.as_double() / 50.0, DynamicUnit("%"));
}

std::optional<Measure> ooxml::read_width_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  const char *type = node.attribute("w:type").value();
  if (std::strcmp("auto", type) == 0) {
    return {};
  }
  if (std::strcmp("dxa", type) == 0) {
    return read_twips_attribute(node.attribute("w:w"));
  }
  if (std::strcmp("nil", type) == 0) {
    return Measure(0, DynamicUnit(""));
  }
  if (std::strcmp("pct", type) == 0) {
    return read_pct_attribute(node.attribute("w:w"));
  }
  return {};
}

/// [ECMA-376] 17.17.4 ST_OnOff, as an attribute that is off when absent.
bool ooxml::read_on_off_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return false;
  }
  const char *value = attribute.value();
  return std::strcmp("0", value) != 0 && std::strcmp("false", value) != 0 &&
         std::strcmp("off", value) != 0;
}

/// [ECMA-376] 17.17.4 ST_OnOff, as an element that is on unless `w:val` says
/// otherwise.
bool ooxml::read_on_off_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  const pugi::xml_attribute value = node.attribute("w:val");
  if (!value) {
    return true;
  }
  return read_on_off_attribute(value);
}

bool ooxml::read_line_attribute(const pugi::xml_node node) {
  if (!node) {
    return false;
  }
  return line_from_value(node.attribute("w:val").value());
}

bool ooxml::read_line_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return false;
  }
  return line_from_value(attribute.value());
}

std::optional<std::string>
ooxml::read_shadow_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return "1pt 1pt";
}

std::optional<std::string>
ooxml::read_shadow_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return "1pt 1pt";
}

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return font_weight_from_value(node.attribute("w:val").value());
}

std::optional<FontWeight>
ooxml::read_font_weight_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return font_weight_from_value(attribute.value());
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return font_style_from_value(node.attribute("w:val").value());
}

std::optional<FontStyle>
ooxml::read_font_style_attribute(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return font_style_from_value(attribute.value());
}

/// [ECMA-376] 17.18.44 ST_Jc. `start`/`end` are relative to the direction.
std::optional<TextAlign>
ooxml::read_text_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("left", val) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("right", val) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("start", val) == 0) {
    return TextAlign::start;
  }
  if (std::strcmp("end", val) == 0) {
    return TextAlign::end;
  }
  if (std::strcmp("center", val) == 0) {
    return TextAlign::center;
  }
  if (std::strcmp("justify", val) == 0) {
    return TextAlign::justify;
  }
  return {};
}

/// [ECMA-376] 20.1.10.59 ST_TextAlignType — drawingml spells the same values
/// differently than wordprocessingml does.
std::optional<TextAlign>
ooxml::read_drawing_text_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("l", val) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("r", val) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("ctr", val) == 0) {
    return TextAlign::center;
  }
  if (std::strcmp("just", val) == 0) {
    return TextAlign::justify;
  }
  return {};
}

std::optional<TextDirection>
ooxml::read_text_direction_attribute(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  return read_on_off_attribute(node) ? TextDirection::right_to_left
                                     : TextDirection::left_to_right;
}

std::optional<TextDirection> ooxml::read_drawing_text_direction_attribute(
    const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  return read_on_off_attribute(attribute) ? TextDirection::right_to_left
                                          : TextDirection::left_to_right;
}

std::optional<VerticalAlign>
ooxml::read_vertical_align_attribute(const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("top", val) == 0) {
    return VerticalAlign::top;
  }
  if (std::strcmp("center", val) == 0) {
    return VerticalAlign::middle;
  }
  if (std::strcmp("bottom", val) == 0) {
    return VerticalAlign::bottom;
  }
  return {};
}

/// [ECMA-376] 20.1.10.60 ST_TextAnchoringType — drawingml spells the same
/// values differently than wordprocessingml does.
std::optional<VerticalAlign> ooxml::read_drawing_vertical_align_attribute(
    const pugi::xml_attribute attribute) {
  const char *val = attribute.value();
  if (std::strcmp("t", val) == 0) {
    return VerticalAlign::top;
  }
  if (std::strcmp("ctr", val) == 0) {
    return VerticalAlign::middle;
  }
  if (std::strcmp("b", val) == 0) {
    return VerticalAlign::bottom;
  }
  return {};
}

std::optional<std::string> ooxml::read_border_node(const pugi::xml_node node) {
  if (!node) {
    return {};
  }
  const char *val = node.attribute("w:val").value();
  if (std::strcmp("nil", val) == 0 || std::strcmp("none", val) == 0) {
    return "0 none";
  }
  const std::optional<Measure> size =
      read_eighth_point_attribute(node.attribute("w:sz"));
  if (!size.has_value()) {
    return {};
  }
  // `auto` reads as no color, which css then takes from the text
  std::string result = size->to_string() + " solid";
  if (const std::optional<Color> color =
          read_color_attribute(node.attribute("w:color"))) {
    result.append(" ").append(html::color(*color));
  }
  return result;
}

DirectionalStyle<std::string>
ooxml::read_borders_node(const pugi::xml_node node) {
  DirectionalStyle<std::string> result;
  result.right = read_border_node(node.child("w:right"));
  result.top = read_border_node(node.child("w:top"));
  result.left = read_border_node(node.child("w:left"));
  result.bottom = read_border_node(node.child("w:bottom"));
  return result;
}

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const pugi::xml_document &relations) {
  std::unordered_map<std::string, std::string> result;
  for (const pugi::xpath_node e : relations.select_nodes("//Relationship")) {
    const std::string r_id = e.node().attribute("Id").as_string();
    const std::string p = e.node().attribute("Target").as_string();
    result.insert({r_id, p});
  }
  return result;
}

namespace {

/// The root is not a part, so it has no parent to hang `_rels` off.
bool is_package_root(const AbsPath &path) { return path == AbsPath("/"); }

AbsPath relationships_path(const AbsPath &path) {
  if (is_package_root(path)) {
    return AbsPath("/_rels/.rels");
  }
  return path.parent()
      .join(RelPath("_rels"))
      .join(RelPath(path.basename() + ".rels"));
}

/// [ECMA-376] 15.2.4: a target is relative to the part that states it, unless
/// it names a part from the package root. One that is empty, or that climbs out
/// of the package, names nothing.
std::optional<AbsPath> resolve_relationship_target(const AbsPath &path,
                                                   const char *target) {
  if (target == nullptr || *target == '\0') {
    return {};
  }
  if (*target == '/') {
    return AbsPath(target);
  }
  try {
    const AbsPath base = is_package_root(path) ? path : path.parent();
    return base.join(RelPath(target));
  } catch (const std::invalid_argument &) {
    return {};
  }
}

} // namespace

std::unordered_map<std::string, std::string>
ooxml::parse_relationships(const abstract::ReadableFilesystem &filesystem,
                           const AbsPath &path) {
  const AbsPath rel_path = relationships_path(path);
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const pugi::xml_document relationships = xml::parse(filesystem, rel_path);
  return parse_relationships(relationships);
}

/// The target of the first relationship whose type ends in @p type
/// (`slideLayout`, `slideMaster`, `theme`, …), resolved against the part.
std::optional<AbsPath>
ooxml::parse_relationship_target(const abstract::ReadableFilesystem &filesystem,
                                 const AbsPath &path,
                                 const std::string_view type) {
  const AbsPath rel_path = relationships_path(path);
  if (!filesystem.is_file(rel_path)) {
    return {};
  }

  const pugi::xml_document relationships = xml::parse(filesystem, rel_path);
  for (const pugi::xpath_node e :
       relationships.select_nodes("//Relationship")) {
    // the type is a uri, so `type` has to match a whole trailing segment
    const std::string_view relation_type =
        e.node().attribute("Type").as_string();
    if (!relation_type.ends_with(type) || relation_type.size() == type.size() ||
        relation_type[relation_type.size() - type.size() - 1] != '/') {
      continue;
    }
    return resolve_relationship_target(
        path, e.node().attribute("Target").as_string());
  }
  return {};
}

} // namespace odr::internal
