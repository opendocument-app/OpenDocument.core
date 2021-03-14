#include <cstring>
#include <glog/logging.h>
#include <internal/odf/odf_translator_context.h>
#include <internal/odf/odf_translator_style.h>
#include <internal/util/string_util.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace odr::internal::odf {

namespace {
void style_properties_translator(const pugi::xml_attribute &in,
                                 std::ostream &out) {
  static std::unordered_map<std::string, const char *> substitution{
      {"fo:text-align", "text-align"},
      {"fo:font-size", "font-size"},
      {"fo:font-weight", "font-weight"},
      {"fo:font-style", "font-style"},
      {"fo:font-size", "font-size"},
      {"fo:text-shadow", "text-shadow"},
      {"fo:color", "color"},
      {"fo:background-color", "background-color"},
      {"fo:page-width", "width"},
      {"fo:page-height", "height"},
      {"fo:margin-top", "margin-top"},
      {"fo:margin-right", "margin-right"},
      {"fo:margin-bottom", "margin-bottom"},
      {"fo:margin-left", "margin-left"},
      {"fo:padding", "padding"},
      {"fo:padding-top", "padding-top"},
      {"fo:padding-right", "padding-right"},
      {"fo:padding-bottom", "padding-bottom"},
      {"fo:padding-left", "padding-left"},
      {"fo:border", "border"},
      {"fo:border-top", "border-top"},
      {"fo:border-right", "border-right"},
      {"fo:border-bottom", "border-bottom"},
      {"fo:border-left", "border-left"},
      {"style:font-name", "font-family"},
      {"style:width", "width"},
      {"style:height", "height"},
      {"style:vertical-align", "vertical-align"},
      {"style:column-width", "width"},
      {"style:row-height", "height"},
      {"draw:fill-color", "fill"},
      {"svg:stroke-color", "stroke"},
      {"svg:stroke-width", "stroke-width"},
      {"text:display", "display"},
  };

  const std::string property = in.name();
  const auto it = substitution.find(property);
  if (it != substitution.end()) {
    out << it->second << ":" << in.as_string() << ";";
  } else if (property == "style:text-underline-style") {
    // TODO breaks line-through
    if (std::strcmp(in.as_string(), "solid") == 0)
      out << "text-decoration:underline;";
  } else if (property == "style:text-line-through-style") {
    // TODO breaks underline
    if (std::strcmp(in.as_string(), "solid") == 0)
      out << "text-decoration:line-through;";
  } else if (property == "draw:textarea-vertical-align") {
    if (std::strcmp(in.as_string(), "middle") == 0)
      out << "display:flex;justify-content:center;flex-direction: column;";
  }
}

void style_class_translator(const pugi::xml_node &in, std::ostream &out,
                            Context &context) {
  static std::unordered_map<std::string, const char *> element_to_name_attr{
      {"style:default-style", "style:family"},
      {"style:style", "style:name"},
      {"style:page-layout", "style:name"},
      {"style:master-page", "style:name"},
  };

  const std::string element = in.name();
  const auto it = element_to_name_attr.find(element);
  if (it == element_to_name_attr.end()) {
    return;
  }

  const auto name_attr = in.attribute(it->second);
  if (!name_attr) {
    LOG(WARNING) << "skipped style " << in.name() << ". no name attribute.";
    return;
  }
  std::string name = style_translator::escape_style_name(name_attr.as_string());
  // master page
  if (std::strcmp(in.name(), "style:master-page") == 0) {
    name = style_translator::escape_master_style_name(name_attr.as_string());
  }

  if (const auto parent_style_name_attr =
          in.attribute("style:parent-style-name");
      parent_style_name_attr) {
    context.style_dependencies[name].push_back(
        style_translator::escape_style_name(
            parent_style_name_attr.as_string()));
  }
  if (const auto family_attr = in.attribute("style:family"); family_attr) {
    context.style_dependencies[name].push_back(
        style_translator::escape_style_name(family_attr.as_string()));
  }

  // master page
  if (const auto page_layout_attr = in.attribute("style:page-layout-name");
      page_layout_attr) {
    context.style_dependencies[name].push_back(
        style_translator::escape_style_name(page_layout_attr.as_string()));
  }
  // master page
  if (const auto draw_style_attr = in.attribute("draw:style-name");
      draw_style_attr) {
    context.style_dependencies[name].push_back(
        style_translator::escape_style_name(draw_style_attr.as_string()));
  }

  out << "." << name << "." << name << " {";

  for (auto &&e : in) {
    for (auto &&a : e.attributes()) {
      style_properties_translator(a, out);
    }
  }

  out << "}\n";
}

// TODO
void list_style_translator(const pugi::xml_node &in, std::ostream &,
                           Context &context) {
  // addElementDelegation("text:list-level-style-number", propertiesTranslator);
  // addElementDelegation("text:list-level-style-bullet", propertiesTranslator);

  const auto style_name_attr = in.parent().attribute("style:name");
  if (style_name_attr == nullptr) {
    LOG(WARNING) << "skipped style " << in.parent().name()
                 << ". no name attribute.";
    return;
  }
  const std::string style_name =
      style_translator::escape_style_name(style_name_attr.as_string());
  context.style_dependencies[style_name] = {};

  const auto list_level_attr = in.attribute("text:level");
  if (!list_level_attr) {
    LOG(WARNING) << "cannot find level attribute";
    return;
  }
  const std::uint32_t listLevel = list_level_attr.as_uint();

  std::string selector = "ul." + style_name;
  for (std::uint32_t i = 1; i < listLevel; ++i) {
    selector += " li";
  }

  const auto bullet_char_attr = in.attribute("text:bullet-char");
  const auto num_format_attr = in.attribute("text:num-format");
  if (bullet_char_attr) {
    *context.output << selector << " {";
    *context.output << "list-style: none;";
    *context.output << "}\n";
    *context.output << selector << " li:before {";
    *context.output << "content: \"" << bullet_char_attr.as_string() << "\";";
    *context.output << "}\n";
  } else if (num_format_attr != nullptr) {
    // TODO check attribute value and switch
    *context.output << selector << " {";
    *context.output << "list-style: decimal;";
    *context.output << "}\n";
  } else {
    LOG(WARNING) << "unhandled case";
  }
}
} // namespace

std::string style_translator::escape_style_name(const std::string &name) {
  std::string result = name;
  util::string::replace_all(result, ".", "_");
  return result;
}

std::string
style_translator::escape_master_style_name(const std::string &name) {
  return "master_" + escape_style_name(name);
}

void style_translator::css(const pugi::xml_node &in, Context &context) {
  for (auto &&e : in) {
    style_class_translator(e, *context.output, context);
  }
}

} // namespace odr::internal::odf
