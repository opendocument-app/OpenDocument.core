#include <odr/internal/odf/odf_slide_layout.hpp>

#include <odr/quantity.hpp>

#include <odr/internal/common/blank_slide_layouts.hpp>
#include <odr/internal/odf/odf_blank.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

/// x, y, width and height of @p placeholder in centimetres, scaled from the
/// built-in page onto a page of @p width by @p height.
std::array<double, 4> scaled(const BlankPlaceholder &placeholder,
                             const double width, const double height) {
  const double scale_x = width / blank_slide_width;
  const double scale_y = height / blank_slide_height;
  return {placeholder.x * scale_x, placeholder.y * scale_y,
          placeholder.width * scale_x, placeholder.height * scale_y};
}

/// The `presentation:placeholder`s of @p layout that take text and state a
/// position.
std::vector<pugi::xml_node> text_placeholders(const pugi::xml_node layout) {
  std::vector<pugi::xml_node> result;
  for (const pugi::xml_node placeholder :
       layout.children("presentation:placeholder")) {
    // ODF 1.3 part 3, 19.427 `presentation:object`
    const std::string_view object =
        placeholder.attribute("presentation:object").value();
    if ((object == "title" || object == "subtitle" || object == "outline" ||
         object == "text") &&
        placeholder.attribute("svg:x") && placeholder.attribute("svg:y") &&
        placeholder.attribute("svg:width") &&
        placeholder.attribute("svg:height")) {
      result.push_back(placeholder);
    }
  }
  return result;
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::vector<odf::SlideLayoutSource>
odf::list_slide_layouts(const pugi::xml_node styles) {
  std::vector<SlideLayoutSource> result;
  const auto count_named = [&](const std::string &name) {
    return std::ranges::count_if(result, [&](const SlideLayoutSource &source) {
      return source.name == name || source.name.starts_with(name + " (");
    });
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
    // two layouts may hold the same placeholders
    if (const auto count = count_named(name); count > 0) {
      name += fmt::format(" ({})", count + 1);
    }
    result.push_back(
        {.name = std::move(name), .node = node, .builtin = nullptr});
  }
  for (const BlankSlideLayout &builtin : blank_slide_layouts) {
    if (count_named(std::string(builtin.name)) == 0) {
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

  for (const BlankPlaceholder &placeholder : layout.placeholders) {
    pugi::xml_node child = node.append_child("presentation:placeholder");
    child.append_attribute("presentation:object")
        .set_value(std::string(presentation_class(placeholder.kind)).c_str());
    constexpr std::array attributes{"svg:x", "svg:y", "svg:width",
                                    "svg:height"};
    for (const auto [attribute, value] :
         std::views::zip(attributes, scaled(placeholder, width, height))) {
      child.append_attribute(attribute).set_value(centimetres(value).c_str());
    }
  }
  return node;
}

void odf::write_slide(pugi::xml_node page, const std::string &name,
                      const std::string &master, const pugi::xml_node layout,
                      const pugi::xml_node styles) {
  page.append_attribute("draw:name").set_value(name.c_str());
  page.append_attribute("draw:master-page-name").set_value(master.c_str());
  page.append_attribute("presentation:presentation-page-layout-name")
      .set_value(layout.attribute("style:name").value());

  for (const pugi::xml_node placeholder : text_placeholders(layout)) {
    const std::string_view object =
        placeholder.attribute("presentation:object").value();
    pugi::xml_node frame = page.append_child("draw:frame");
    // LibreOffice names a master's presentation styles `<master>-<kind>`
    const std::string style = master + (object == "title"      ? "-title"
                                        : object == "subtitle" ? "-subtitle"
                                                               : "-outline1");
    if (styles.find_child_by_attribute("style:style", "style:name",
                                       style.c_str())) {
      frame.append_attribute("presentation:style-name")
          .set_value(style.c_str());
    }
    for (const char *attribute :
         {"svg:x", "svg:y", "svg:width", "svg:height"}) {
      frame.append_attribute(attribute).set_value(
          placeholder.attribute(attribute).value());
    }
    frame.append_attribute("presentation:class")
        .set_value(std::string(object).c_str());
    frame.append_child("draw:text-box").append_child("text:p");
  }
}

void odf::write_text_box(pugi::xml_node frame, const std::string &style,
                         const Measure &x, const Measure &y,
                         const Measure &width, const Measure &height) {
  frame.append_attribute("draw:style-name").set_value(style.c_str());
  frame.append_attribute("draw:layer").set_value("layout");
  frame.append_attribute("svg:width").set_value(width.to_string().c_str());
  frame.append_attribute("svg:height").set_value(height.to_string().c_str());
  frame.append_attribute("svg:x").set_value(x.to_string().c_str());
  frame.append_attribute("svg:y").set_value(y.to_string().c_str());
  frame.append_child("draw:text-box").append_child("text:p");
}

} // namespace odr::internal
