#include <odr/internal/odf/odf_list.hpp>

#include <odr/document_element.hpp>

#include <odr/internal/common/list_numbering.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {

ListNumberFormat parse_number_format(const char *format) {
  const std::string value = format != nullptr ? format : "";

  if (value.empty()) {
    return ListNumberFormat::none;
  }
  if (value == "a") {
    return ListNumberFormat::letter_lower;
  }
  if (value == "A") {
    return ListNumberFormat::letter_upper;
  }
  if (value == "i") {
    return ListNumberFormat::roman_lower;
  }
  if (value == "I") {
    return ListNumberFormat::roman_upper;
  }
  // Anything else is decimal; a leading zero asks for the padded variant.
  return value.front() == '0' ? ListNumberFormat::decimal_zero
                              : ListNumberFormat::decimal;
}

/// The level style for `level` (counted from 0), or the deepest one defined.
pugi::xml_node level_style_node(const pugi::xml_node list_style,
                                const std::uint32_t level) {
  pugi::xml_node result;
  for (const pugi::xml_node child : list_style.children()) {
    const auto child_level = child.attribute("text:level").as_uint(1);
    if (child_level == level + 1) {
      return child;
    }
    if (!result || child_level > result.attribute("text:level").as_uint(1)) {
      result = child;
    }
  }
  return result;
}

ListLevel read_level(const pugi::xml_node level_style,
                     const std::uint32_t level) {
  ListLevel result;

  const std::string name = level_style.name();

  if (name == "text:list-level-style-number") {
    result.format =
        parse_number_format(level_style.attribute("style:num-format").value());
    result.start = level_style.attribute("text:start-value").as_uint(1);

    // ODF spells the label out as prefix, the numbers of the last
    // `text:display-levels` levels joined by a period, then suffix.
    const auto display_levels =
        level_style.attribute("text:display-levels").as_uint(1);
    const std::uint32_t first =
        display_levels > level ? 0 : level - display_levels + 1;

    result.label = level_style.attribute("style:num-prefix").value();
    for (std::uint32_t i = first; i <= level; ++i) {
      if (i > first) {
        result.label += ".";
      }
      result.label += "%" + std::to_string(i + 1);
    }
    result.label += level_style.attribute("style:num-suffix").value();

    return result;
  }

  result.format = ListNumberFormat::bullet;
  if (name == "text:list-level-style-bullet") {
    result.label = level_style.attribute("text:bullet-char").value();
  }
  if (result.label.empty()) {
    // A bullet without a character, and the image variant, still want a mark.
    result.label = "•";
  }
  return result;
}

bool is_ordered(const ListNumberFormat format) {
  return format != ListNumberFormat::bullet && format != ListNumberFormat::none;
}

class Resolver final {
public:
  Resolver(ElementRegistry &registry, const StyleRegistry &styles)
      : m_registry{&registry}, m_styles{&styles} {}

  void walk(const ElementIdentifier id) {
    for (ElementIdentifier child_id = id; child_id != null_element_id;
         child_id = m_registry->element_at(child_id).next_sibling_id) {
      const ElementRegistry::Element &element =
          m_registry->element_at(child_id);

      switch (element.type) {
      case ElementType::list:
        walk_list_(child_id, element);
        break;
      case ElementType::list_item:
        walk_list_item_(child_id, element);
        break;
      default:
        walk(element.first_child_id);
        break;
      }
    }
  }

private:
  ElementRegistry *m_registry{nullptr};
  const StyleRegistry *m_styles{nullptr};

  std::unordered_map<std::string, ListCounter> m_counters;
  std::vector<std::string> m_open_lists;

  [[nodiscard]] ListLevel level_at_(const std::string &style_name,
                                    const std::uint32_t level) const {
    return read_level(
        level_style_node(m_styles->list_style_node(style_name), level), level);
  }

  void walk_list_(const ElementIdentifier id,
                  const ElementRegistry::Element &element) {
    const auto level = static_cast<std::uint32_t>(m_open_lists.size());

    // A nested list usually carries no style of its own and stays on the one
    // its ancestor opened.
    std::string style_name = element.node.attribute("text:style-name").value();
    if (style_name.empty() && !m_open_lists.empty()) {
      style_name = m_open_lists.back();
    }

    // Only an outermost list restarts; a nested one continues under the
    // counters its parent item just reset.
    if (level == 0 &&
        !element.node.attribute("text:continue-numbering").as_bool()) {
      m_counters.erase(style_name);
    }

    m_registry->set_list_type(id,
                              is_ordered(level_at_(style_name, level).format)
                                  ? ListType::ordered
                                  : ListType::unordered);

    m_open_lists.push_back(std::move(style_name));
    walk(element.first_child_id);
    m_open_lists.pop_back();
  }

  void walk_list_item_(const ElementIdentifier id,
                       const ElementRegistry::Element &element) {
    if (!m_open_lists.empty() &&
        std::string(element.node.name()) != "text:list-header") {
      const auto level = static_cast<std::uint32_t>(m_open_lists.size() - 1);
      const std::string &style_name = m_open_lists.back();

      ListLevel list_level = level_at_(style_name, level);

      ListCounter &counter = m_counters[style_name];
      if (const pugi::xml_attribute start =
              element.node.attribute("text:start-value")) {
        list_level.start = start.as_uint(1);
        counter.restart(level, 0);
      }

      ListMarker marker;
      marker.text = counter.advance(level, list_level);
      if (is_ordered(list_level.format)) {
        marker.number = counter.number(level);
      }
      m_registry->set_list_marker(id, std::move(marker));
    }

    walk(element.first_child_id);
  }
};

} // namespace

void resolve_list_numbering(ElementRegistry &registry,
                            const StyleRegistry &styles,
                            const ElementIdentifier root_id) {
  if (root_id == null_element_id) {
    return;
  }
  Resolver{registry, styles}.walk(registry.element_at(root_id).first_child_id);
}

} // namespace odr::internal::odf
