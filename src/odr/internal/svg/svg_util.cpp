#include <odr/internal/svg/svg_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/xml/xml_file.hpp>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>
#include <vector>

namespace odr::internal {

namespace {

using namespace std::string_view_literals;

/// In a page these run code or fetch something, and none draws anything.
constexpr std::array dropped_elements{
    "script"sv, "foreignObject"sv, "handler"sv, "iframe"sv,   "embed"sv,
    "object"sv, "audio"sv,         "video"sv,   "animation"sv};

/// SMIL animates any attribute it is pointed at, including the ones below.
constexpr std::array animation_elements{"set"sv, "animate"sv,
                                        "animateTransform"sv, "animateMotion"sv,
                                        "animateColor"sv};

constexpr std::array reference_attributes{"href"sv, "src"sv};

/// pugixml does not process namespaces, so the prefix comes off by hand.
std::string_view local_name(std::string_view name) {
  if (const std::size_t colon = name.find(':');
      colon != std::string_view::npos) {
    name.remove_prefix(colon + 1);
  }
  return name;
}

char lower(const char c) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

/// Case-insensitively, because the html parser lowercases what it reads in
/// foreign content - `<SCRIPT>` arrives in the page as a script element.
bool equals_ignore_case(const std::string_view a, const std::string_view b) {
  return std::ranges::equal(
      a, b, [](const char x, const char y) { return lower(x) == lower(y); });
}

bool starts_with_ignore_case(const std::string_view value,
                             const std::string_view prefix) {
  return value.size() >= prefix.size() &&
         equals_ignore_case(value.substr(0, prefix.size()), prefix);
}

std::size_t find_ignore_case(const std::string_view haystack,
                             const std::string_view needle,
                             const std::size_t from) {
  if (from > haystack.size()) {
    return std::string_view::npos;
  }
  const std::string_view rest = haystack.substr(from);
  const auto found =
      std::ranges::search(rest, needle, [](const char x, const char y) {
        return lower(x) == lower(y);
      });
  if (found.empty()) {
    return std::string_view::npos;
  }
  return from + static_cast<std::size_t>(found.begin() - rest.begin());
}

/// Same-document, or an image carried in the url itself. Anything else is
/// either code or a fetch.
bool reference_is_safe(std::string_view value) {
  // url parsers ignore leading whitespace and control characters, so
  // `\njavascript:` is `javascript:`
  while (!value.empty() && static_cast<unsigned char>(value.front()) <= ' ') {
    value.remove_prefix(1);
  }
  return value.starts_with('#') ||
         starts_with_ignore_case(value, "data:image/");
}

bool css_is_safe(const std::string_view css) {
  if (find_ignore_case(css, "@import", 0) != std::string_view::npos) {
    return false;
  }

  for (std::size_t at = find_ignore_case(css, "url(", 0);
       at != std::string_view::npos;
       at = find_ignore_case(css, "url(", at + 1)) {
    std::string_view target = css.substr(at + 4);
    while (!target.empty() &&
           static_cast<unsigned char>(target.front()) <= ' ') {
      target.remove_prefix(1);
    }
    if (!target.empty() && (target.front() == '"' || target.front() == '\'')) {
      target.remove_prefix(1);
    }
    if (!reference_is_safe(target)) {
      return false;
    }
  }
  return true;
}

bool is_in(const auto &names, const std::string_view name) {
  return std::ranges::any_of(names, [name](const std::string_view candidate) {
    return equals_ignore_case(name, candidate);
  });
}

bool is_event_handler(const std::string_view name) {
  return starts_with_ignore_case(name, "on");
}

bool drop_element(const pugi::xml_node &node) {
  const std::string_view name = local_name(node.name());

  if (is_in(dropped_elements, name)) {
    return true;
  }
  if (is_in(animation_elements, name)) {
    const std::string_view target =
        local_name(node.attribute("attributeName").value());
    return is_event_handler(target) || is_in(reference_attributes, target);
  }
  if (equals_ignore_case(name, "style")) {
    return !css_is_safe(node.text().get());
  }
  return false;
}

std::size_t sanitize_attributes(pugi::xml_node node) {
  std::vector<pugi::xml_attribute> dropped;

  for (const pugi::xml_attribute &attribute : node.attributes()) {
    const std::string_view name = local_name(attribute.name());

    if (is_event_handler(name) ||
        (is_in(reference_attributes, name) &&
         !reference_is_safe(attribute.value())) ||
        (equals_ignore_case(name, "style") &&
         !css_is_safe(attribute.value()))) {
      dropped.push_back(attribute);
    }
  }

  for (const pugi::xml_attribute &attribute : dropped) {
    node.remove_attribute(attribute);
  }
  return dropped.size();
}

} // namespace

void svg::check_svg_file(const xml::XmlFile &file) {
  if (local_name(file.root_name()) != "svg") {
    throw NoSvgFile();
  }
}

std::size_t svg::sanitize(pugi::xml_node node) {
  std::size_t removed = sanitize_attributes(node);

  std::vector<pugi::xml_node> dropped;
  for (const pugi::xml_node &child : node.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (drop_element(child)) {
      dropped.push_back(child);
      continue;
    }
    removed += sanitize(child);
  }

  for (const pugi::xml_node &child : dropped) {
    node.remove_child(child);
  }
  return removed + dropped.size();
}

} // namespace odr::internal
