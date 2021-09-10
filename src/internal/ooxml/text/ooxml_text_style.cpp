#include <cstring>
#include <internal/abstract/document.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <internal/util/property_util.h>

namespace odr::internal::ooxml::text {

Style::Style(std::string name, pugi::xml_node node, const Style *parent)
    : m_name{std::move(name)}, m_node{node}, m_parent{parent} {}

std::string Style::name() const { return m_name; }

const Style *Style::parent() const { return m_parent; }

const common::ResolvedStyle &Style::resolved() const { return m_resolved; }

void Style::resolve_style_() {
  resolve_text_style_(m_node, m_resolved.text_style);
  resolve_paragraph_style_(m_node, m_resolved.paragraph_style);
  resolve_table_style_(m_node, m_resolved.table_style);
  resolve_table_column_style_(m_node, m_resolved.table_column_style);
  resolve_table_row_style_(m_node, m_resolved.table_row_style);
  resolve_table_cell_style_(m_node, m_resolved.table_cell_style);
  resolve_graphic_style_(m_node, m_resolved.graphic_style);
}

void Style::resolve_text_style_(pugi::xml_node node,
                                std::optional<TextStyle> &result) {
  if (auto run_properties = node.child("w:rPr")) {
    if (!result) {
      result = TextStyle();
    }

    result->font_name = read_string_attribute(
        run_properties.child("w:rFonts").attribute("w:ascii"));
    result->font_size =
        read_string_attribute(run_properties.child("w:sz").attribute("w:val"));
    result->font_weight =
        read_string_attribute(run_properties.child("w:b").attribute("w:val"));
    result->font_style =
        read_string_attribute(run_properties.child("w:i").attribute("w:val"));
    result->font_underline = read_optional_attribute(
        run_properties.child("w:u").attribute("w:val"), read_line_attribute);
    result->font_line_through = read_optional_attribute(
        run_properties.child("w:strike").attribute("w:val"),
        read_line_attribute);
    result->font_shadow = read_optional_node(run_properties.child("w:shadow"),
                                             read_shadow_attribute);
    result->font_color = read_optional_attribute(
        run_properties.child("w:b").attribute("w:val"), read_color_attribute);
    result->background_color = read_optional_attribute(
        run_properties.child("w:highlight").attribute("w:val"),
        read_color_attribute);
  }
}

void Style::resolve_paragraph_style_(pugi::xml_node node,
                                     std::optional<ParagraphStyle> &result) {
  if (auto paragraph_properties = node.child("style:paragraph-properties")) {
    if (!result) {
      result = ParagraphStyle();
    }

    /*
    result->text_align =
        read_optional_attribute(node.child("w:pPr").child("w:jc").attribute("w:val"));
        */
  }
}

void Style::resolve_table_style_(pugi::xml_node node,
                                 std::optional<TableStyle> &result) {
  if (auto table_properties = node.child("style:table-properties")) {
    if (!result) {
      result = TableStyle();
    }
  }
}

void Style::resolve_table_column_style_(
    pugi::xml_node node, std::optional<TableColumnStyle> &result) {
  if (auto table_column_properties =
          node.child("style:table-column-properties")) {
    if (!result) {
      result = TableColumnStyle();
    }
  }
}

void Style::resolve_table_row_style_(pugi::xml_node node,
                                     std::optional<TableRowStyle> &result) {
  if (auto table_row_properties = node.child("style:table-row-properties")) {
    if (!result) {
      result = TableRowStyle();
    }
  }
}

void Style::resolve_table_cell_style_(pugi::xml_node node,
                                      std::optional<TableCellStyle> &result) {
  if (auto table_cell_properties = node.child("style:table-cell-properties")) {
    if (!result) {
      result = TableCellStyle();
    }
  }
}

void Style::resolve_graphic_style_(pugi::xml_node node,
                                   std::optional<GraphicStyle> &result) {
  if (auto graphic_properties = node.child("style:graphic-properties")) {
    if (!result) {
      result = GraphicStyle();
    }
  }
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
    parent =
        generate_style_(parent_attr.value(), m_index.at(parent_attr.value()));
  }

  style = std::make_unique<Style>(name, node, parent);
  return style.get();
}

} // namespace odr::internal::ooxml::text
