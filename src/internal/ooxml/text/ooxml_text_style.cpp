#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <internal/util/property_util.h>

namespace odr::internal::ooxml {

namespace {
void resolve_paragraph_style_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  const auto paragraph_properties = "w:pPr";

  const auto properties_node = node.child(paragraph_properties);
  if (!properties_node) {
    return;
  }

  util::property::set_optional_property(
      ElementProperty::TEXT_ALIGN,
      read_optional_attribute(properties_node.child("w:jc").attribute("w:val")),
      result);
}

void resolve_text_style_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  const auto run_properties = "w:rPr";

  const auto properties_node = node.child(run_properties);
  if (!properties_node) {
    return;
  }

  util::property::set_optional_property(
      ElementProperty::FONT_NAME,
      read_optional_attribute(
          properties_node.child("w:rFonts").attribute("w:ascii")),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_SIZE,
      read_optional_attribute(properties_node.child("w:sz").attribute("w:val"),
                              read_half_point_attribute),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_COLOR,
      read_optional_attribute(
          properties_node.child("w:color").attribute("w:val"),
          read_color_attribute),
      result);
  util::property::set_optional_property(
      ElementProperty::BACKGROUND_COLOR,
      read_optional_attribute(
          properties_node.child("w:highlight").attribute("w:val"),
          read_color_attribute),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_WEIGHT,
      read_optional_node(properties_node.child("w:b"),
                         [](const pugi::xml_node) { return std::any("bold"); }),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_STYLE,
      read_optional_node(
          properties_node.child("w:i"),
          [](const pugi::xml_node) { return std::any("italic"); }),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_UNDERLINE,
      read_optional_attribute(properties_node.child("w:u").attribute("w:val"),
                              read_line_attribute),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_STRIKETHROUGH,
      read_optional_attribute(
          properties_node.child("w:strike").attribute("w:val"),
          read_line_attribute),
      result);
  util::property::set_optional_property(
      ElementProperty::FONT_STRIKETHROUGH,
      read_optional_node(properties_node.child("w:shadow"),
                         read_shadow_attribute),
      result);
}

void resolve_style_properties(
    const ElementType element, const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  if (element == ElementType::PARAGRAPH) {
    resolve_paragraph_style_properties(node, result);
    resolve_text_style_properties(node, result);
  } else if (element == ElementType::SPAN) {
    resolve_text_style_properties(node, result);
  }
}
} // namespace

OfficeOpenXmlTextStyle::OfficeOpenXmlTextStyle() = default;

OfficeOpenXmlTextStyle::OfficeOpenXmlTextStyle(
    const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_();
}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlTextStyle::resolve_style(const ElementType element_type,
                                      const pugi::xml_node element) const {
  std::unordered_map<ElementProperty, std::any> result;

  pugi::xml_node properties;
  std::string style_name;

  // TODO move to element
  // TODO use map?
  if (element_type == ElementType::PARAGRAPH) {
    properties = element.child("w:pPr");
    style_name = properties.child("w:pStyle").attribute("w:val").as_string();
  } else if (element_type == ElementType::SPAN) {
    properties = element.child("w:rPr");
    style_name = properties.child("w:rStyle").attribute("w:val").as_string();
  }

  if (!style_name.empty()) {
    result[ElementProperty::STYLE_NAME] = style_name;
  }

  if (auto style_it = m_styles.find(style_name);
      style_it != std::end(m_styles)) {
    style_it->second->properties(element_type, result);
  }

  resolve_style_properties(element_type, element, result);

  return result;
}

OfficeOpenXmlTextStyle::Entry::Entry(std::shared_ptr<Entry> parent,
                                     pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

void OfficeOpenXmlTextStyle::Entry::properties(
    const ElementType element,
    std::unordered_map<ElementProperty, std::any> &result) const {
  if (m_parent) {
    m_parent->properties(element, result);
  }

  resolve_style_properties(element, m_node, result);
}

void OfficeOpenXmlTextStyle::generate_indices_(
    const pugi::xml_node styles_root) {
  for (auto style : styles_root) {
    std::string element_name = style.name();

    if (element_name == "w:style") {
      m_index[style.attribute("w:styleId").value()] = style;
    }
  }
}

void OfficeOpenXmlTextStyle::generate_styles_() {
  for (auto &&e : m_index) {
    generate_style_(e.first, e.second);
  }
}

std::shared_ptr<OfficeOpenXmlTextStyle::Entry>
OfficeOpenXmlTextStyle::generate_style_(const std::string &name,
                                        const pugi::xml_node node) {
  if (auto style_it = m_styles.find(name); style_it != std::end(m_styles)) {
    return style_it->second;
  }

  std::shared_ptr<Entry> parent;

  // TODO use defaults as parent?

  if (auto parent_attr = node.child("w:basedOn").attribute("w:val");
      parent_attr) {
    if (auto parent_style_it = m_index.find(parent_attr.value());
        parent_style_it != std::end(m_index)) {
      parent = generate_style_(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Entry>(parent, node);
}

} // namespace odr::internal::ooxml
