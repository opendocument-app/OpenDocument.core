#include <odr/internal/odf/odf_list.hpp>

#include <odr/document_element.hpp>

#include <odr/internal/common/list_numbering.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <algorithm>
#include <string>
#include <string_view>
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

/// Level @p level (counted from 0) of @p list_style, as LibreOffice labels it.
ListLevel read_level(const pugi::xml_node list_style,
                     const std::uint32_t level) {
  ListLevel result;

  const pugi::xml_node level_style = level_style_node(list_style, level);
  const std::string name = level_style.name();

  if (name == "text:list-level-style-number" ||
      name == "text:outline-level-style") {
    result.format =
        parse_number_format(level_style.attribute("style:num-format").value());
    result.start = level_style.attribute("text:start-value").as_uint(1);

    result.label = level_style.attribute("style:num-prefix").value();
    if (result.format != ListNumberFormat::none) {
      const auto display_levels =
          level_style.attribute("text:display-levels").as_uint(1);
      const std::uint32_t first =
          display_levels > level ? 0 : level - display_levels + 1;
      bool joined = false;
      for (std::uint32_t i = first; i <= level; ++i) {
        if (i < level &&
            parse_number_format(level_style_node(list_style, i)
                                    .attribute("style:num-format")
                                    .value()) == ListNumberFormat::none) {
          continue;
        }
        if (joined) {
          result.label += ".";
        }
        result.label += "%{" + std::to_string(i + 1) + "}";
        joined = true;
      }
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

/// LibreOffice's outline has ten levels and reads a deeper one as the tenth.
constexpr std::uint32_t max_outline_level = 10;

bool is_ordered(const ListNumberFormat format) {
  return format != ListNumberFormat::bullet && format != ListNumberFormat::none;
}

/// Opens each level above @p level that no item has opened yet at its start.
void start_skipped_levels(ListCounter &counter, const pugi::xml_node list_style,
                          const std::uint32_t level) {
  for (std::uint32_t skipped = 0; skipped < level; ++skipped) {
    counter.start(skipped, read_level(list_style, skipped));
  }
}

/// Whether @p list_item has a paragraph to number before any nested list.
bool has_own_paragraph(const pugi::xml_node list_item) {
  for (const pugi::xml_node child : list_item.children()) {
    const std::string_view name = child.name();
    if (name == "text:p" || name == "text:h") {
      return true;
    }
    if (name == "text:list") {
      return false;
    }
  }
  return false;
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
      case ElementType::paragraph:
        if (m_open_lists.empty()) {
          number_heading_(child_id, element);
        }
        walk(element.first_child_id);
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
  /// The headings' counters, which run through the whole document.
  ListCounter m_outline;
  std::vector<std::string> m_open_lists;

  [[nodiscard]] ListLevel level_at_(const std::string &style_name,
                                    const std::uint32_t level) const {
    return read_level(m_styles->list_style_node(style_name), level);
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

  /// Numbers a heading after the outline style at its `text:outline-level`.
  void number_heading_(const ElementIdentifier id,
                       const ElementRegistry::Element &element) {
    const pugi::xml_node node = element.node;
    if (std::string_view(node.name()) != "text:h" ||
        node.attribute("text:is-list-header").as_bool()) {
      return;
    }
    const std::uint32_t outline_level =
        node.attribute("text:outline-level").as_uint(1);
    const pugi::xml_node outline_style = m_styles->outline_style_node();
    if (outline_level == 0 || !outline_style) {
      return;
    }
    // the level sizes the counters, so a file must not choose it freely
    const std::uint32_t level = std::min(outline_level, max_outline_level) - 1;

    ListLevel list_level = read_level(outline_style, level);
    if (node.attribute("text:restart-numbering").as_bool()) {
      list_level.start =
          node.attribute("text:start-value").as_uint(list_level.start);
      m_outline.restart(level, 0);
    }

    start_skipped_levels(m_outline, outline_style, level);
    ListMarker marker;
    marker.text = m_outline.advance(level, list_level);
    if (!marker.text.empty()) {
      m_registry->set_list_marker(id, std::move(marker));
    }
  }

  void walk_list_item_(const ElementIdentifier id,
                       const ElementRegistry::Element &element) {
    if (!m_open_lists.empty() &&
        std::string(element.node.name()) != "text:list-header" &&
        has_own_paragraph(element.node)) {
      const auto level = static_cast<std::uint32_t>(m_open_lists.size() - 1);
      const std::string &style_name = m_open_lists.back();

      ListLevel list_level = level_at_(style_name, level);

      ListCounter &counter = m_counters[style_name];
      if (const pugi::xml_attribute start =
              element.node.attribute("text:start-value")) {
        list_level.start = start.as_uint(1);
        counter.restart(level, 0);
      }

      start_skipped_levels(counter, m_styles->list_style_node(style_name),
                           level);
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
