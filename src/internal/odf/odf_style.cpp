#include <functional>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
std::any fetch_string_property(const pugi::xml_attribute attribute) {
  if (attribute) {
    return attribute.value();
  }
  return {};
}

void resolve_property_class(
    const pugi::xml_node property_class,
    const std::unordered_map<std::string, ElementProperty> &string_properties,
    std::unordered_map<ElementProperty, std::any> &result) {
  if (!property_class) {
    return;
  }

  for (auto attribute : property_class.attributes()) {
    if (auto it = string_properties.find(attribute.name());
        it != std::end(string_properties)) {
      result[it->second] = fetch_string_property(attribute);
    }
  }
}

void resolve_text_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"style:font-name", ElementProperty::FONT_NAME},
      {"fo:font-size", ElementProperty::FONT_SIZE},
      {"fo:font-weight", ElementProperty::FONT_WEIGHT},
      {"fo:font-style", ElementProperty::FONT_STYLE},
      {"style:text-underline-style", ElementProperty::FONT_UNDERLINE},
      {"style:text-line-through-style", ElementProperty::FONT_STRIKETHROUGH},
      {"fo:text-shadow", ElementProperty::FONT_SHADOW},
      {"fo:color", ElementProperty::FONT_COLOR},
      {"fo:background-color", ElementProperty::BACKGROUND_COLOR},
  };

  resolve_property_class(node, string_properties, result);
}

void resolve_paragraph_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"fo:text-align", ElementProperty::TEXT_ALIGN},
      {"fo:margin-top", ElementProperty::MARGIN_TOP},
      {"fo:margin-bottom", ElementProperty::MARGIN_BOTTOM},
      {"fo:margin-left", ElementProperty::MARGIN_LEFT},
      {"fo:margin-right", ElementProperty::MARGIN_RIGHT},
  };

  result[ElementProperty::MARGIN_TOP] = result[ElementProperty::MARGIN_BOTTOM] =
      result[ElementProperty::MARGIN_LEFT] =
          result[ElementProperty::MARGIN_RIGHT] =
              fetch_string_property(node.attribute("fo:margin"));

  resolve_property_class(node, string_properties, result);
}

void resolve_table_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"style:width", ElementProperty::WIDTH},
  };

  resolve_property_class(node, string_properties, result);
}

void resolve_table_column_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"style:column-width", ElementProperty::WIDTH},
  };

  resolve_property_class(node, string_properties, result);
}

void resolve_table_row_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"style:row-height", ElementProperty::HEIGHT},
  };

  resolve_property_class(node, string_properties, result);
}

void resolve_table_cell_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"style:vertical-align", ElementProperty::VERTICAL_ALIGN},
      {"fo:background-color", ElementProperty::TABLE_CELL_BACKGROUND_COLOR},
      {"fo:padding-top", ElementProperty::PADDING_TOP},
      {"fo:padding-bottom", ElementProperty::PADDING_BOTTOM},
      {"fo:padding-left", ElementProperty::PADDING_LEFT},
      {"fo:padding-right", ElementProperty::PADDING_RIGHT},
      {"fo:border-top", ElementProperty::BORDER_TOP},
      {"fo:border-bottom", ElementProperty::BORDER_BOTTOM},
      {"fo:border-left", ElementProperty::BORDER_LEFT},
      {"fo:border-right", ElementProperty::BORDER_RIGHT},
  };

  result[ElementProperty::PADDING_TOP] =
      result[ElementProperty::PADDING_BOTTOM] =
          result[ElementProperty::PADDING_LEFT] =
              result[ElementProperty::PADDING_RIGHT] =
                  fetch_string_property(node.attribute("fo:padding"));
  result[ElementProperty::BORDER_TOP] = result[ElementProperty::BORDER_BOTTOM] =
      result[ElementProperty::BORDER_LEFT] =
          result[ElementProperty::BORDER_RIGHT] =
              fetch_string_property(node.attribute("fo:border"));

  resolve_property_class(node, string_properties, result);
}

void resolve_graphic_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"svg:stroke-width", ElementProperty::STROKE_WIDTH},
      {"svg:stroke-color", ElementProperty::STROKE_COLOR},
      {"draw:fill-color", ElementProperty::FILL_COLOR},
      {"draw:textarea-vertical-align", ElementProperty::VERTICAL_ALIGN},
  };

  resolve_property_class(node, string_properties, result);
}

void resolve_page_layout_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  static std::unordered_map<std::string, ElementProperty> string_properties{
      {"fo:page-width", ElementProperty::WIDTH},
      {"fo:page-height", ElementProperty::HEIGHT},
      {"style:print-orientation", ElementProperty::PRINT_ORIENTATION},
      {"fo:margin-top", ElementProperty::MARGIN_TOP},
      {"fo:margin-bottom", ElementProperty::MARGIN_BOTTOM},
      {"fo:margin-left", ElementProperty::MARGIN_LEFT},
      {"fo:margin-right", ElementProperty::MARGIN_RIGHT},
  };

  result[ElementProperty::MARGIN_TOP] = result[ElementProperty::MARGIN_BOTTOM] =
      result[ElementProperty::MARGIN_LEFT] =
          result[ElementProperty::MARGIN_RIGHT] =
              fetch_string_property(node.attribute("fo:margin"));

  resolve_property_class(node, string_properties, result);
}

void resolve_properties(const ElementType element_type,
                        const pugi::xml_node node,
                        std::unordered_map<ElementProperty, std::any> &result) {
  // TODO it would be better if the element selects the properties

  switch (element_type) {
  case ElementType::PARAGRAPH:
    resolve_text_properties(node.child("style:text-properties"), result);
    resolve_paragraph_properties(node.child("style:paragraph-properties"),
                                 result);
    break;
  case ElementType::SPAN:
    resolve_text_properties(node.child("style:text-properties"), result);
    break;
  case ElementType::LINK:
    resolve_text_properties(node.child("style:text-properties"), result);
    break;
  case ElementType::TABLE:
    resolve_table_properties(node.child("style:table-properties"), result);
    break;
  case ElementType::TABLE_COLUMN:
    resolve_table_column_properties(node.child("style:table-column-properties"),
                                    result);
    break;
  case ElementType::TABLE_ROW:
    resolve_table_row_properties(node.child("style:table-row-properties"),
                                 result);
    break;
  case ElementType::TABLE_CELL:
    resolve_text_properties(node.child("style:text-properties"), result);
    resolve_paragraph_properties(node.child("style:paragraph-properties"),
                                 result);
    resolve_table_cell_properties(node.child("style:table-cell-properties"),
                                  result);
    break;
  case ElementType::RECT:
  case ElementType::LINE:
  case ElementType::CIRCLE:
    resolve_graphic_properties(node.child("style:graphic-properties"), result);
    break;
  case ElementType::ROOT:
  case ElementType::SLIDE:
  case ElementType::PAGE:
    resolve_page_layout_properties(node.child("style:page-layout-properties"),
                                   result);
    break;
  default:
    break;
  }
}
} // namespace

Style::Entry::Entry(std::shared_ptr<Entry> parent, pugi::xml_node node)
    : parent{std::move(parent)}, node{node} {}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
Style::Entry::properties(const ElementType element) const {
  std::unordered_map<ElementProperty, std::any> result;

  if (parent) {
    result = parent->properties(element);
  }

  resolve_properties(element, node, result);

  return result;
}

Style::Style() = default;

Style::Style(const pugi::xml_node content_root,
             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

void Style::generate_indices_(const pugi::xml_node content_root,
                              const pugi::xml_node styles_root) {
  if (auto font_face_decls = styles_root.child("office:font-face-decls")) {
    generate_indices_(font_face_decls);
  }

  if (auto styles = styles_root.child("office:styles")) {
    generate_indices_(styles);
  }

  if (auto automatic_styles = styles_root.child("office:automatic-styles")) {
    generate_indices_(automatic_styles);
  }

  if (auto master_styles = styles_root.child("office:master-styles")) {
    generate_indices_(master_styles);
  }

  // content styles

  if (auto font_face_decls = content_root.child("office:font-face-decls")) {
    generate_indices_(font_face_decls);
  }

  if (auto automatic_styles = content_root.child("office:automatic-styles")) {
    generate_indices_(automatic_styles);
  }
}

void Style::generate_indices_(const pugi::xml_node node) {
  for (auto &&e : node) {
    std::string name = e.name();

    if (name == "style:font-face") {
      m_index_font_face[e.attribute("style:name").value()] = e;
    } else if (name == "style:default-style") {
      m_index_default_style[e.attribute("style:family").value()] = e;
    } else if (name == "style:style") {
      m_index_style[e.attribute("style:name").value()] = e;
    } else if (name == "style:list-style") {
      m_index_list_style[e.attribute("style:name").value()] = e;
    } else if (name == "style:outline-style") {
      m_index_outline_style[e.attribute("style:name").value()] = e;
    } else if (name == "style:page-layout") {
      m_index_page_layout[e.attribute("style:name").value()] = e;
    } else if (name == "style:master-page") {
      std::string master_page_name = e.attribute("style:name").value();
      m_index_master_page[master_page_name] = e;
      if (!m_first_master_page) {
        m_first_master_page = master_page_name;
      }
    }
  }
}

void Style::generate_styles_() {
  for (auto &&e : m_index_default_style) {
    generate_default_style_(e.first, e.second);
  }

  for (auto &&e : m_index_style) {
    generate_style_(e.first, e.second);
  }

  for (auto &&e : m_index_page_layout) {
    generate_page_layout_(e.first, e.second);
  }
}

std::shared_ptr<Style::Entry>
Style::generate_default_style_(const std::string &name,
                               const pugi::xml_node node) {
  if (auto it = m_default_styles.find(name); it != std::end(m_default_styles)) {
    return it->second;
  }

  return m_default_styles[name] = std::make_shared<Entry>(nullptr, node);
}

std::shared_ptr<Style::Entry>
Style::generate_style_(const std::string &name, const pugi::xml_node node) {
  if (auto style_it = m_styles.find(name); style_it != std::end(m_styles)) {
    return style_it->second;
  }

  std::shared_ptr<Entry> parent;

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    if (auto parent_style_it = m_index_style.find(parent_attr.value());
        parent_style_it != std::end(m_index_style)) {
      parent = generate_style_(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  } else if (auto family_attr = node.attribute("style:family"); family_attr) {
    if (auto family_style_it = m_index_default_style.find(family_attr.value());
        family_style_it != std::end(m_index_default_style)) {
      parent =
          generate_default_style_(family_attr.value(), family_style_it->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Entry>(parent, node);
}

std::shared_ptr<Style::Entry>
Style::generate_page_layout_(const std::string &name,
                             const pugi::xml_node node) {
  if (auto it = m_page_layouts.find(name); it != std::end(m_page_layouts)) {
    return it->second;
  }

  return m_page_layouts[name] = std::make_shared<Entry>(nullptr, node);
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_style(const ElementType element,
                     const std::string &style_name) const {
  if (auto style_it = m_styles.find(style_name);
      style_it != std::end(m_styles)) {
    return style_it->second->properties(element);
  }
  return {};
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_page_layout(const ElementType element,
                           const std::string &page_layout_name) const {
  if (auto page_layout_it = m_page_layouts.find(page_layout_name);
      page_layout_it != std::end(m_page_layouts)) {
    return page_layout_it->second->properties(element);
  }
  return {};
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_master_page(const ElementType element,
                           const std::string &master_page_name) const {
  auto master_page_it = m_index_master_page.find(master_page_name);
  if (master_page_it == std::end(m_index_master_page)) {
    return {};
  }

  std::unordered_map<ElementProperty, std::any> result;
  if (auto page_layout_name_attr =
          master_page_it->second.attribute("style:page-layout-name")) {
    auto style = resolve_page_layout(element, page_layout_name_attr.value());
    result.insert(std::begin(style), std::end(style));
  }
  if (auto style_name_attr =
          master_page_it->second.attribute("draw:style-name")) {
    auto style = resolve_style(element, style_name_attr.value());
    result.insert(std::begin(style), std::end(style));
  }
  return result;
}

pugi::xml_node
Style::master_page_node(const std::string &master_page_name) const {

  if (auto master_page_it = m_index_master_page.find(master_page_name);
      master_page_it != std::end(m_index_master_page)) {
    return master_page_it->second;
  }
  return {};
}

std::optional<std::string> Style::first_master_page() const {
  return m_first_master_page;
}

} // namespace odr::internal::odf
