#include <StyleTranslator.h>
#include <common/StringUtil.h>
#include <cstring>
#include <glog/logging.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace odr {
namespace odf {

namespace {
void StylePropertiesTranslator(const pugi::xml_attribute &in,
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

void StyleClassTranslator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  static std::unordered_map<std::string, const char *> elementToNameAttr{
      {"style:default-style", "style:family"},
      {"style:style", "style:name"},
      {"style:page-layout", "style:name"},
      {"style:master-page", "style:name"},
  };

  const std::string element = in.name();
  const auto it = elementToNameAttr.find(element);
  if (it == elementToNameAttr.end())
    return;

  const auto nameAttr = in.attribute(it->second);
  if (!nameAttr) {
    LOG(WARNING) << "skipped style " << in.name() << ". no name attribute.";
    return;
  }
  std::string name = StyleTranslator::escapeStyleName(nameAttr.as_string());
  // master page
  if (std::strcmp(in.name(), "style:master-page") == 0)
    name = StyleTranslator::escapeMasterStyleName(nameAttr.as_string());

  if (const auto parentStyleNameAttr = in.attribute("style:parent-style-name");
      parentStyleNameAttr) {
    context.styleDependencies[name].push_back(
        StyleTranslator::escapeStyleName(parentStyleNameAttr.as_string()));
  }
  if (const auto familyAttr = in.attribute("style:family"); familyAttr) {
    context.styleDependencies[name].push_back(
        StyleTranslator::escapeStyleName(familyAttr.as_string()));
  }

  // master page
  if (const auto pageLayoutAttr = in.attribute("style:page-layout-name");
      pageLayoutAttr) {
    context.styleDependencies[name].push_back(
        StyleTranslator::escapeStyleName(pageLayoutAttr.as_string()));
  }
  // master page
  if (const auto drawStyleAttr = in.attribute("style:style-name");
      drawStyleAttr) {
    context.styleDependencies[name].push_back(
        StyleTranslator::escapeStyleName(drawStyleAttr.as_string()));
  }

  out << "." << name << "." << name << " {";

  for (auto &&e : in) {
    for (auto &&a : e.attributes()) {
      StylePropertiesTranslator(a, out);
    }
  }

  out << "}\n";
}

// TODO
void ListStyleTranslator(const pugi::xml_node &in, std::ostream &,
                         Context &context) {
  // addElementDelegation("text:list-level-style-number", propertiesTranslator);
  // addElementDelegation("text:list-level-style-bullet", propertiesTranslator);

  const auto styleNameAttr = in.parent().attribute("style:name");
  if (styleNameAttr == nullptr) {
    LOG(WARNING) << "skipped style " << in.parent().name()
                 << ". no name attribute.";
    return;
  }
  const std::string styleName =
      StyleTranslator::escapeStyleName(styleNameAttr.as_string());
  context.styleDependencies[styleName] = {};

  const auto listLevelAttr = in.attribute("text:level");
  if (!listLevelAttr) {
    LOG(WARNING) << "cannot find level attribute";
    return;
  }
  const std::uint32_t listLevel = listLevelAttr.as_uint();

  std::string selector = "ul." + styleName;
  for (std::uint32_t i = 1; i < listLevel; ++i) {
    selector += " li";
  }

  const auto bulletCharAttr = in.attribute("text:bullet-char");
  const auto numFormatAttr = in.attribute("text:num-format");
  if (bulletCharAttr) {
    *context.output << selector << " {";
    *context.output << "list-style: none;";
    *context.output << "}\n";
    *context.output << selector << " li:before {";
    *context.output << "content: \"" << bulletCharAttr.as_string() << "\";";
    *context.output << "}\n";
  } else if (numFormatAttr != nullptr) {
    // TODO check attribute value and switch
    *context.output << selector << " {";
    *context.output << "list-style: decimal;";
    *context.output << "}\n";
  } else {
    LOG(WARNING) << "unhandled case";
  }
}
} // namespace

std::string StyleTranslator::escapeStyleName(const std::string &name) {
  std::string result = name;
  common::StringUtil::findAndReplaceAll(result, ".", "_");
  return result;
}

std::string StyleTranslator::escapeMasterStyleName(const std::string &name) {
  return "master_" + escapeStyleName(name);
}

void StyleTranslator::css(const pugi::xml_node &in, Context &context) {
  for (auto &&e : in) {
    StyleClassTranslator(e, *context.output, context);
  }
}

} // namespace odf
} // namespace odr
