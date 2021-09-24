#include <internal/abstract/document.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_style.h>

namespace odr::internal::ooxml::text {

namespace {
void resolve_text_style_(pugi::xml_node node,
                         std::optional<TextStyle> &result) {
  if (auto run_properties = node.child("w:rPr")) {
    if (!result) {
      result = TextStyle();
    }

    if (auto font_name =
            run_properties.child("w:rFonts").attribute("w:ascii")) {
      result->font_name = font_name.value();
    }
    if (auto font_size = read_half_point_attribute(
            run_properties.child("w:sz").attribute("w:val"))) {
      result->font_size = font_size;
    }
    if (auto font_weight =
            read_font_weight_attribute(run_properties.child("w:b"))) {
      result->font_weight = font_weight;
    }
    if (auto font_style =
            read_font_style_attribute(run_properties.child("w:i"))) {
      result->font_style = font_style;
    }
    if (auto font_underline =
            read_line_attribute(run_properties.child("w:u"))) {
      result->font_underline = font_underline;
    }
    if (auto font_line_through =
            read_line_attribute(run_properties.child("w:strike"))) {
      result->font_line_through = font_line_through;
    }
    if (auto font_shadow =
            read_shadow_attribute(run_properties.child("w:shadow"))) {
      result->font_shadow = font_shadow;
    }
    if (auto font_color = read_color_attribute(
            run_properties.child("w:color").attribute("w:val"))) {
      result->font_color = font_color;
    }
    if (auto background_color = read_color_attribute(
            run_properties.child("w:highlight").attribute("w:val"))) {
      result->background_color = background_color;
    }
  }
}

void resolve_paragraph_style_(pugi::xml_node node,
                              std::optional<ParagraphStyle> &result) {
  if (auto paragraph_properties = node.child("w:pPr")) {
    if (!result) {
      result = ParagraphStyle();
    }

    if (auto text_align =
            read_text_align_attribute(paragraph_properties.child("w:jc"))) {
      result->text_align = text_align;
    }
    if (auto margin_left = read_twips_attribute(
            paragraph_properties.child("w:ind").attribute("w:left"))) {
      result->margin.left = margin_left;
    }
    if (auto margin_left = read_twips_attribute(
            paragraph_properties.child("w:ind").attribute("w:start"))) {
      result->margin.left = margin_left;
    }
    if (auto margin_right = read_twips_attribute(
            paragraph_properties.child("w:ind").attribute("w:right"))) {
      result->margin.right = margin_right;
    }
    if (auto margin_right = read_twips_attribute(
            paragraph_properties.child("w:ind").attribute("w:end"))) {
      result->margin.right = margin_right;
    }
  }
}

void resolve_table_style_(pugi::xml_node node,
                          std::optional<TableStyle> &result) {
  if (auto table_properties = node.child("w:tblPr")) {
    if (!result) {
      result = TableStyle();
    }

    if (auto width = read_width_attribute(table_properties.child("w:tblW"))) {
      result->width = width;
    }
  }
}

void resolve_table_row_style_(pugi::xml_node node,
                              std::optional<TableRowStyle> &result) {
  if (auto table_row_properties = node.child("w:trPr")) {
    if (!result) {
      result = TableRowStyle();
    }
  }
}

void resolve_table_cell_style_(pugi::xml_node node,
                               std::optional<TableCellStyle> &result) {
  if (auto table_cell_properties = node.child("w:tcPr")) {
    if (!result) {
      result = TableCellStyle();
    }

    if (auto width =
            read_width_attribute(table_cell_properties.child("w:tcW"))) {
      // result->width = width; // TODO
    }
    if (auto vertical_align = read_vertical_align_attribute(
            table_cell_properties.child("w:vAlign").attribute("w:val"))) {
      result->vertical_align = vertical_align;
    }
    if (auto border_right = read_border_attribute(
            table_cell_properties.child("w:tcBorders").child("w:right"))) {
      result->border.right = border_right;
    }
    if (auto border_top = read_border_attribute(
            table_cell_properties.child("w:tcBorders").child("w:top"))) {
      result->border.top = border_top;
    }
    if (auto border_left = read_border_attribute(
            table_cell_properties.child("w:tcBorders").child("w:left"))) {
      result->border.left = border_left;
    }
    if (auto border_bottom = read_border_attribute(
            table_cell_properties.child("w:tcBorders").child("w:bottom"))) {
      result->border.bottom = border_bottom;
    }
  }
}

void resolve_graphic_style_(pugi::xml_node, std::optional<GraphicStyle> &) {
  // TODO
}
} // namespace

Style::Style(std::string name, pugi::xml_node node, const Style *parent)
    : m_name{std::move(name)}, m_node{node}, m_parent{parent} {
  resolve_style_();
}

std::string Style::name() const { return m_name; }

const Style *Style::parent() const { return m_parent; }

const common::ResolvedStyle &Style::resolved() const { return m_resolved; }

void Style::resolve_style_() {
  resolve_text_style_(m_node, m_resolved.text_style);
  resolve_paragraph_style_(m_node, m_resolved.paragraph_style);
  resolve_table_style_(m_node, m_resolved.table_style);
  resolve_table_row_style_(m_node, m_resolved.table_row_style);
  resolve_table_cell_style_(m_node, m_resolved.table_cell_style);
  resolve_graphic_style_(m_node, m_resolved.graphic_style);
}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_();
}

Style *StyleRegistry::style(const std::string &name) const {
  if (auto styles_it = m_styles.find(name); styles_it != std::end(m_styles)) {
    return styles_it->second.get();
  }
  return nullptr;
}

common::ResolvedStyle
StyleRegistry::partial_text_style(const pugi::xml_node node) const {
  if (!node) {
    return {};
  }
  common::ResolvedStyle result;
  if (auto style_name =
          node.child("w:rPr").child("w:rStyle").attribute("w:val")) {
    if (auto style = this->style(style_name.value())) {
      result = style->resolved();
    }
  }
  resolve_text_style_(node, result.text_style);
  return result;
}

common::ResolvedStyle
StyleRegistry::partial_paragraph_style(const pugi::xml_node node) const {
  if (!node) {
    return {};
  }
  common::ResolvedStyle result;
  if (auto style_name =
          node.child("w:pPr").child("w:pStyle").attribute("w:val")) {
    if (auto style = this->style(style_name.value())) {
      result = style->resolved();
    }
  }
  resolve_paragraph_style_(node, result.paragraph_style);
  if (auto text_style = partial_text_style(node.child("w:pPr")).text_style) {
    if (!result.text_style) {
      result.text_style = TextStyle();
    }
    result.text_style->override(*text_style);
  }
  return result;
}

common::ResolvedStyle
StyleRegistry::partial_table_style(const pugi::xml_node node) const {
  if (!node) {
    return {};
  }
  common::ResolvedStyle result;
  resolve_table_style_(node, result.table_style);
  return result;
}

common::ResolvedStyle
StyleRegistry::partial_table_row_style(const pugi::xml_node) const {
  return {};
}

common::ResolvedStyle
StyleRegistry::partial_table_cell_style(const pugi::xml_node node) const {
  if (!node) {
    return {};
  }
  common::ResolvedStyle result;
  resolve_table_cell_style_(node, result.table_cell_style);
  return result;
}

void StyleRegistry::generate_indices_(const pugi::xml_node styles_root) {
  for (auto style : styles_root) {
    std::string element_name = style.name();

    if (element_name == "w:style") {
      m_index[style.attribute("w:styleId").value()] = style;
    }
  }
}

void StyleRegistry::generate_styles_() {
  for (auto &&e : m_index) {
    generate_style_(e.first, e.second);
  }
}

Style *StyleRegistry::generate_style_(const std::string &name,
                                      const pugi::xml_node node) {
  auto &&style = m_styles[name];
  if (style) {
    return style.get();
  }

  Style *parent{nullptr};

  if (auto parent_attr = node.child("w:basedOn").attribute("w:val");
      parent_attr) {
    if (auto parent_node = m_index[parent_attr.value()]) {
      parent = generate_style_(parent_attr.value(), parent_node);
    }
  }

  style = std::make_unique<Style>(name, node, parent);
  return style.get();
}

} // namespace odr::internal::ooxml::text
