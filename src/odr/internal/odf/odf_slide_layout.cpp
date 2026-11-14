#include <odr/internal/odf/odf_slide_layout.hpp>

#include <odr/internal/common/blank_slide_layouts.hpp>
#include <odr/internal/odf/odf_blank.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace odr::internal::odf {

namespace {

/// The objects a layout holds, sorted, so a set compares as a whole.
std::vector<std::string> objects_of(const pugi::xml_node layout) {
  std::vector<std::string> result;
  for (const pugi::xml_node placeholder :
       layout.children("presentation:placeholder")) {
    result.emplace_back(placeholder.attribute("presentation:object").value());
  }
  std::ranges::sort(result);
  return result;
}

std::vector<std::string> objects_of(const BlankSlideLayout &layout) {
  std::vector<std::string> result;
  for (const BlankPlaceholder &placeholder : layout.placeholders) {
    result.emplace_back(presentation_class(placeholder.kind));
  }
  std::ranges::sort(result);
  return result;
}

/// LibreOffice names a layout `AL<n>T<m>` and shows it by what it holds, so
/// one that holds what a built-in holds takes the built-in's name.
std::string derived_name(const pugi::xml_node layout) {
  const std::vector<std::string> objects = objects_of(layout);
  for (const BlankSlideLayout &builtin : blank_slide_layouts) {
    if (objects_of(builtin) == objects) {
      return std::string(builtin.name);
    }
  }
  return layout.attribute("style:name").value();
}

bool for_handout_or_notes(const pugi::xml_node layout) {
  constexpr std::array<std::string_view, 3> kinds{"handout", "notes", "page"};
  return std::ranges::any_of(
      layout.children("presentation:placeholder"), [&](const auto placeholder) {
        return std::ranges::find(
                   kinds,
                   std::string_view(
                       placeholder.attribute("presentation:object").value())) !=
               kinds.end();
      });
}

std::string centimetres(const double value) {
  return fmt::format("{}cm", util::number::to_string_significant(value, 6));
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::vector<odf::SlideLayout>
odf::list_slide_layouts(const pugi::xml_node styles) {
  std::vector<SlideLayout> result;
  const auto named = [&](const std::string &name) {
    return std::ranges::any_of(
        result, [&](const SlideLayout &layout) { return layout.name == name; });
  };

  for (const pugi::xml_node node :
       styles.children("style:presentation-page-layout")) {
    if (for_handout_or_notes(node)) {
      continue;
    }
    std::string name = node.attribute("style:display-name").value();
    if (name.empty()) {
      name = derived_name(node);
    }
    if (!named(name)) {
      result.push_back(
          {.name = std::move(name), .node = node, .builtin = nullptr});
    }
  }
  for (const BlankSlideLayout &builtin : blank_slide_layouts) {
    if (!named(std::string(builtin.name))) {
      result.push_back({.name = std::string(builtin.name),
                        .node = pugi::xml_node(),
                        .builtin = &builtin});
    }
  }
  return result;
}

pugi::xml_node odf::write_slide_layout(pugi::xml_node styles,
                                       const BlankSlideLayout &layout,
                                       const double width,
                                       const double height) {
  std::string name;
  for (std::uint32_t number = 1;; ++number) {
    name = fmt::format("odrL{}", number);
    if (!styles.find_child_by_attribute("style:presentation-page-layout",
                                        "style:name", name.c_str())) {
      break;
    }
  }

  pugi::xml_node node = styles.append_child("style:presentation-page-layout");
  node.append_attribute("style:name").set_value(name.c_str());
  node.append_attribute("style:display-name")
      .set_value(std::string(layout.name).c_str());

  const double scale_x = width * 100.0 / blank_slide_width;
  const double scale_y = height * 100.0 / blank_slide_height;
  for (const BlankPlaceholder &placeholder : layout.placeholders) {
    pugi::xml_node child = node.append_child("presentation:placeholder");
    child.append_attribute("presentation:object")
        .set_value(std::string(presentation_class(placeholder.kind)).c_str());
    const std::array<std::pair<const char *, double>, 4> geometry{{
        {"svg:x", placeholder.x * scale_x / 100.0},
        {"svg:y", placeholder.y * scale_y / 100.0},
        {"svg:width", placeholder.width * scale_x / 100.0},
        {"svg:height", placeholder.height * scale_y / 100.0},
    }};
    for (const auto &[attribute, value] : geometry) {
      child.append_attribute(attribute).set_value(centimetres(value).c_str());
    }
  }
  return node;
}

} // namespace odr::internal
