#include <odr/internal/ooxml/text/ooxml_text_list.hpp>

#include <odr/document_element.hpp>

#include <odr/internal/ooxml/text/ooxml_text_element_registry.hpp>

#include <odr/internal/util/string_util.hpp>

#include <array>
#include <charconv>
#include <limits>

#include <utf8/checked.h>

namespace odr::internal::ooxml::text {

namespace {

/// How deep `w:numStyleLink` is followed before giving up on a cycle.
constexpr std::uint32_t max_numbering_indirection = 4;

ListNumberFormat parse_number_format(const std::string &format) {
  if (format == "bullet") {
    return ListNumberFormat::bullet;
  }
  if (format == "none") {
    return ListNumberFormat::none;
  }
  if (format == "upperRoman") {
    return ListNumberFormat::roman_upper;
  }
  if (format == "lowerRoman") {
    return ListNumberFormat::roman_lower;
  }
  if (format == "upperLetter") {
    return ListNumberFormat::letter_upper;
  }
  if (format == "lowerLetter") {
    return ListNumberFormat::letter_lower;
  }
  if (format == "decimalZero") {
    return ListNumberFormat::decimal_zero;
  }
  return ListNumberFormat::decimal;
}

/// A decimal in [0, @p maximum]. A missing or invalid value gives @p fallback,
/// as LibreOffice ignores it, so one bad attribute does not refuse the file.
std::uint32_t read_number(
    const pugi::xml_attribute attribute, const std::uint32_t fallback,
    const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) {
  if (!attribute) {
    return fallback;
  }
  auto value = util::string::trim_view(attribute.value());
  if (value.starts_with('+')) {
    value.remove_prefix(1);
  }
  std::uint32_t result{};
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (error != std::errc{} || end != value.data() + value.size() ||
      result > maximum) {
    return fallback;
  }
  return result;
}

/// Map private-use symbol-font bullets to Unicode.
std::string resolve_bullet(const std::string &text, const std::uint32_t level) {
  static constexpr std::array<const char *, 3> defaults{"•", "◦", "▪"};

  if (text.empty() || !utf8::is_valid(text.begin(), text.end())) {
    return defaults[level % defaults.size()];
  }

  auto it = std::begin(text);
  const char32_t first = utf8::next(it, text.end());
  if (first < 0xE000 || first > 0xF8FF) {
    return text;
  }

  switch (first) {
  case 0xF06E:
    return "■";
  case 0xF075:
    return "❖";
  case 0xF0A7:
    return "▪";
  case 0xF0A8:
    return "◆";
  case 0xF0D8:
    return "➢";
  case 0xF0FC:
    return "✔";
  default:
    return defaults[level % defaults.size()];
  }
}

pugi::xml_node level_node(const pugi::xml_node abstract_numbering,
                          const std::uint32_t level) {
  pugi::xml_node result;
  for (const pugi::xml_node child : abstract_numbering.children("w:lvl")) {
    const auto child_level = read_number(child.attribute("w:ilvl"), 0, 8);
    if (child_level == level) {
      return child;
    }
    if (!result ||
        child_level > read_number(result.attribute("w:ilvl"), 0, 8)) {
      result = child;
    }
  }
  return result;
}

ListLevel read_level(const pugi::xml_node node, const std::uint32_t level) {
  ListLevel result;
  result.format =
      parse_number_format(node.child("w:numFmt").attribute("w:val").value());
  result.start = read_number(node.child("w:start").attribute("w:val"), 0);
  result.label = node.child("w:lvlText").attribute("w:val").value();

  if (result.format == ListNumberFormat::bullet) {
    result.label = resolve_bullet(result.label, level);
  }
  return result;
}

} // namespace

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

std::uint32_t text::list_level(const pugi::xml_node paragraph) {
  return read_number(
      paragraph.child("w:pPr").child("w:numPr").child("w:ilvl").attribute(
          "w:val"),
      0, 8);
}

} // namespace odr::internal::ooxml

namespace odr::internal::ooxml::text {

NumberingRegistry::NumberingRegistry(const pugi::xml_node numbering_root,
                                     const pugi::xml_node styles_root) {
  for (const pugi::xml_node node : numbering_root.children("w:abstractNum")) {
    m_abstract_numbering[node.attribute("w:abstractNumId").value()] = node;
  }
  for (const pugi::xml_node node : numbering_root.children("w:num")) {
    m_numbering[node.attribute("w:numId").value()] = node;
  }
  for (const pugi::xml_node node : styles_root.children("w:style")) {
    if (const pugi::xml_attribute num_id =
            node.child("w:pPr").child("w:numPr").child("w:numId").attribute(
                "w:val")) {
      m_style_numbering[node.attribute("w:styleId").value()] = num_id.value();
    }
  }
}

pugi::xml_node
NumberingRegistry::abstract_numbering_(const std::string &num_id,
                                       const std::uint32_t depth) const {
  if (depth > max_numbering_indirection) {
    return {};
  }

  const auto numbering_it = m_numbering.find(num_id);
  if (numbering_it == std::end(m_numbering)) {
    return {};
  }

  const auto abstract_it = m_abstract_numbering.find(
      numbering_it->second.child("w:abstractNumId").attribute("w:val").value());
  if (abstract_it == std::end(m_abstract_numbering)) {
    return {};
  }

  // An abstract numbering may only name the style that holds the real one.
  if (const pugi::xml_node link = abstract_it->second.child("w:numStyleLink")) {
    const auto style_it =
        m_style_numbering.find(link.attribute("w:val").value());
    if (style_it != std::end(m_style_numbering) && style_it->second != num_id) {
      if (const pugi::xml_node linked =
              abstract_numbering_(style_it->second, depth + 1)) {
        return linked;
      }
    }
  }

  return abstract_it->second;
}

ListLevel NumberingRegistry::level(const std::string &num_id,
                                   const std::uint32_t level) const {
  ListLevel result;
  result.format = ListNumberFormat::bullet;
  result.label = resolve_bullet("", level);

  const auto numbering_it = m_numbering.find(num_id);
  if (numbering_it == std::end(m_numbering)) {
    return result;
  }

  pugi::xml_node override_node;
  for (const pugi::xml_node node :
       numbering_it->second.children("w:lvlOverride")) {
    if (read_number(node.attribute("w:ilvl"), 0, 8) == level) {
      override_node = node;
      break;
    }
  }

  if (const pugi::xml_node overridden = override_node.child("w:lvl")) {
    result = read_level(overridden, level);
  } else if (const pugi::xml_node node =
                 level_node(abstract_numbering_(num_id, 0), level)) {
    result = read_level(node, level);
  }

  if (const pugi::xml_attribute start =
          override_node.child("w:startOverride").attribute("w:val")) {
    result.start = read_number(start, 0);
  }

  return result;
}

} // namespace odr::internal::ooxml::text

namespace odr::internal::ooxml {

void text::resolve_list_numbering(ElementRegistry &registry,
                                  const NumberingRegistry &numbering,
                                  const ElementIdentifier root_id) {
  if (root_id == null_element_id) {
    return;
  }

  // Word keeps one set of counters per `w:numId`, independent of how the
  // paragraphs nest, so no stack is needed — only document order.
  std::unordered_map<std::string, ListCounter> counters;

  const auto numbering_properties = [](const pugi::xml_node node) {
    return node.child("w:pPr").child("w:numPr");
  };

  // Takes itself as a parameter rather than `this auto self`: NDK 28.1's
  // clang 19 segfaults on a capturing lambda that recurses through one.
  const auto walk = [&](auto &self, const ElementIdentifier id) -> void {
    for (ElementIdentifier child_id = id; child_id != null_element_id;
         child_id = registry.element_at(child_id).next_sibling_id) {
      const ElementRegistry::Element &element = registry.element_at(child_id);

      if (element.type == ElementType::list ||
          element.type == ElementType::list_item) {
        const pugi::xml_node properties = numbering_properties(element.node);
        const std::string num_id =
            properties.child("w:numId").attribute("w:val").value();
        const auto level = list_level(element.node);
        const ListLevel list_level = numbering.level(num_id, level);
        const bool ordered = list_level.format != ListNumberFormat::bullet &&
                             list_level.format != ListNumberFormat::none;

        if (element.type == ElementType::list) {
          registry.set_list_type(child_id, ordered ? ListType::ordered
                                                   : ListType::unordered);
        } else {
          ListMarker marker;
          marker.text = counters[num_id].advance(level, list_level);
          if (ordered) {
            marker.number = counters[num_id].number(level);
          }
          registry.set_list_marker(child_id, std::move(marker));
        }
      }

      self(self, element.first_child_id);
    }
  };

  walk(walk, registry.element_at(root_id).first_child_id);
}

} // namespace odr::internal::ooxml
