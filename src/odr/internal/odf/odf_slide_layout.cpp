#include <odr/internal/odf/odf_slide_layout.hpp>

#include <odr/internal/common/blank_slide_layouts.hpp>
#include <odr/internal/odf/odf_blank.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
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

/// ODF 1.3 part 3, 19.427 `presentation:object`; none for a graphic object.
std::optional<SlidePlaceholderKind> text_kind(const std::string_view object) {
  if (object == "title") {
    return SlidePlaceholderKind::title;
  }
  if (object == "subtitle") {
    return SlidePlaceholderKind::subtitle;
  }
  if (object == "outline" || object == "text") {
    return SlidePlaceholderKind::body;
  }
  return std::nullopt;
}

SlidePlaceholderKind slide_kind(const BlankPlaceholderKind kind) {
  switch (kind) {
  case BlankPlaceholderKind::title:
  case BlankPlaceholderKind::centered_title:
    return SlidePlaceholderKind::title;
  case BlankPlaceholderKind::subtitle:
    return SlidePlaceholderKind::subtitle;
  case BlankPlaceholderKind::content:
  case BlankPlaceholderKind::text:
    return SlidePlaceholderKind::body;
  }
  throw std::invalid_argument("unknown placeholder kind");
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

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::optional<SlidePlaceholder>
odf::read_text_placeholder(const pugi::xml_node node) {
  const std::optional<SlidePlaceholderKind> kind =
      text_kind(node.attribute("presentation:object").value());
  const std::array attributes{node.attribute("svg:x"), node.attribute("svg:y"),
                              node.attribute("svg:width"),
                              node.attribute("svg:height")};
  if (!kind.has_value() ||
      !std::ranges::all_of(attributes, [](const pugi::xml_attribute a) {
        return static_cast<bool>(a);
      })) {
    return std::nullopt;
  }
  return SlidePlaceholder(
      *kind, Measure(attributes[0].value()), Measure(attributes[1].value()),
      Measure(attributes[2].value()), Measure(attributes[3].value()));
}

std::vector<odf::SlideLayoutSource>
odf::list_slide_layouts(const pugi::xml_node styles, const double width,
                        const double height) {
  std::vector<SlideLayoutSource> result;
  const auto count_named = [&](const std::string &name) {
    return std::ranges::count_if(result, [&](const SlideLayoutSource &source) {
      const std::string &taken = source.layout.name();
      return taken == name || taken.starts_with(name + " (");
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
    std::vector<SlidePlaceholder> placeholders;
    for (const pugi::xml_node child :
         node.children("presentation:placeholder")) {
      if (const std::optional<SlidePlaceholder> placeholder =
              read_text_placeholder(child);
          placeholder.has_value()) {
        placeholders.push_back(*placeholder);
      }
    }
    result.push_back(
        {.layout = SlideLayout(std::move(name), std::move(placeholders)),
         .node = node,
         .builtin = nullptr});
  }
  for (const BlankSlideLayout &builtin : blank_slide_layouts) {
    if (count_named(std::string(builtin.name)) > 0) {
      continue;
    }
    std::vector<SlidePlaceholder> placeholders;
    for (const BlankPlaceholder &placeholder : builtin.placeholders) {
      const auto [x, y, w, h] = scaled(placeholder, width, height);
      const DynamicUnit cm("cm");
      placeholders.emplace_back(slide_kind(placeholder.kind), Measure(x, cm),
                                Measure(y, cm), Measure(w, cm), Measure(h, cm));
    }
    result.push_back({.layout = SlideLayout(std::string(builtin.name),
                                            std::move(placeholders)),
                      .node = pugi::xml_node(),
                      .builtin = &builtin});
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

} // namespace odr::internal
