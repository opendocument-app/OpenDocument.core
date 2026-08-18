#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <odr/internal/ooxml/ooxml_util.hpp>

#include <cstring>
#include <ranges>
#include <utility>
#include <vector>

namespace odr::internal::ooxml::text {

namespace {

void resolve_text_style_(const pugi::xml_node node, TextStyle &result) {
  const pugi::xml_node run_properties = node.child("w:rPr");

  if (const pugi::xml_attribute font_name =
          run_properties.child("w:rFonts").attribute("w:ascii")) {
    result.font_name = font_name.value();
  }
  if (const std::optional<Measure> font_size = read_half_point_attribute(
          run_properties.child("w:sz").attribute("w:val"))) {
    result.font_size = font_size;
  }
  if (const std::optional<FontWeight> font_weight =
          read_font_weight_attribute(run_properties.child("w:b"))) {
    result.font_weight = font_weight;
  }
  if (const std::optional<FontStyle> font_style =
          read_font_style_attribute(run_properties.child("w:i"))) {
    result.font_style = font_style;
  }
  if (const bool font_underline =
          read_line_attribute(run_properties.child("w:u"))) {
    result.font_underline = font_underline;
  }
  if (const bool font_line_through =
          read_line_attribute(run_properties.child("w:strike"))) {
    result.font_line_through = font_line_through;
  }
  if (const std::optional<std::string> font_shadow =
          read_shadow_attribute(run_properties.child("w:shadow"))) {
    result.font_shadow = font_shadow;
  }
  if (const std::optional<Color> font_color = read_color_attribute(
          run_properties.child("w:color").attribute("w:val"))) {
    result.font_color = font_color;
  }
  if (const std::optional<Color> background_color = read_color_attribute(
          run_properties.child("w:highlight").attribute("w:val"))) {
    result.background_color = background_color;
  }
}

void resolve_paragraph_style_(const pugi::xml_node node,
                              ParagraphStyle &result) {
  const pugi::xml_node paragraph_properties = node.child("w:pPr");

  if (const std::optional<TextAlign> text_align = read_text_align_attribute(
          paragraph_properties.child("w:jc").attribute("w:val"))) {
    result.text_align = text_align;
  }
  if (const std::optional<Measure> margin_left = read_twips_attribute(
          paragraph_properties.child("w:ind").attribute("w:left"))) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_left = read_twips_attribute(
          paragraph_properties.child("w:ind").attribute("w:start"))) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_right = read_twips_attribute(
          paragraph_properties.child("w:ind").attribute("w:right"))) {
    result.margin.right = margin_right;
  }
  if (const std::optional<Measure> margin_right = read_twips_attribute(
          paragraph_properties.child("w:ind").attribute("w:end"))) {
    result.margin.right = margin_right;
  }

  const pugi::xml_node spacing = paragraph_properties.child("w:spacing");

  // an autospacing flag makes word compute the spacing itself and ignore the
  // value next to it
  if (!read_on_off_attribute(spacing.attribute("w:beforeAutospacing"))) {
    if (const std::optional<Measure> margin_top =
            read_twips_attribute(spacing.attribute("w:before"))) {
      result.margin.top = margin_top;
    }
  }
  if (!read_on_off_attribute(spacing.attribute("w:afterAutospacing"))) {
    if (const std::optional<Measure> margin_bottom =
            read_twips_attribute(spacing.attribute("w:after"))) {
      result.margin.bottom = margin_bottom;
    }
  }
  if (const pugi::xml_attribute line = spacing.attribute("w:line")) {
    // [ECMA-376] 17.3.1.33: `atLeast`/`exact` measure in twips, the default
    // `auto` in 240ths of a line
    const char *line_rule = spacing.attribute("w:lineRule").value();
    if (std::strcmp("atLeast", line_rule) == 0 ||
        std::strcmp("exact", line_rule) == 0) {
      result.line_height = read_twips_attribute(line);
    } else {
      result.line_height = Measure(line.as_double() / 2.4, DynamicUnit("%"));
    }
  }
}

void resolve_table_style_(const pugi::xml_node node, TableStyle &result) {
  const pugi::xml_node table_properties = node.child("w:tblPr");

  if (const std::optional<Measure> width =
          read_width_attribute(table_properties.child("w:tblW"))) {
    result.width = width;
  }
}

void resolve_table_row_style_(const pugi::xml_node node,
                              TableRowStyle &result) {
  const pugi::xml_node table_row_properties = node.child("w:trPr");

  // `auto` makes the row grow with its content, which is what html does anyway;
  // word omits the rule for `atLeast`, so a bare `w:val` is a minimum height
  const pugi::xml_node height = table_row_properties.child("w:trHeight");
  if (std::strcmp("auto", height.attribute("w:hRule").value()) != 0) {
    if (const std::optional<Measure> height_value =
            read_twips_attribute(height.attribute("w:val"))) {
      result.height = height_value;
    }
  }
}

void resolve_table_cell_style_(const pugi::xml_node node,
                               TableCellStyle &result) {
  const pugi::xml_node table_cell_properties = node.child("w:tcPr");

  if (const std::optional<Measure> width =
          read_width_attribute(table_cell_properties.child("w:tcW"))) {
    // result.width = width; // TODO
  }
  if (const std::optional<VerticalAlign> vertical_align =
          read_vertical_align_attribute(
              table_cell_properties.child("w:vAlign").attribute("w:val"))) {
    result.vertical_align = vertical_align;
  }
  if (const std::optional<std::string> border_right = read_border_node(
          table_cell_properties.child("w:tcBorders").child("w:right"))) {
    result.border.right = border_right;
  }
  if (const std::optional<std::string> border_top = read_border_node(
          table_cell_properties.child("w:tcBorders").child("w:top"))) {
    result.border.top = border_top;
  }
  if (const std::optional<std::string> border_left = read_border_node(
          table_cell_properties.child("w:tcBorders").child("w:left"))) {
    result.border.left = border_left;
  }
  if (const std::optional<std::string> border_bottom = read_border_node(
          table_cell_properties.child("w:tcBorders").child("w:bottom"))) {
    result.border.bottom = border_bottom;
  }
}

void resolve_graphic_style_(pugi::xml_node, GraphicStyle &) {
  // TODO
}

/// Whether `node` is a paragraph carrying the paragraph style `style_name`
/// names, an absent name matching an absent `w:pStyle`.
bool has_paragraph_style(const pugi::xml_node node,
                         const pugi::xml_attribute style_name) {
  if (std::strcmp("w:p", node.name()) != 0) {
    return false;
  }
  return std::strcmp(
             node.child("w:pPr").child("w:pStyle").attribute("w:val").value(),
             style_name.value()) == 0;
}

} // namespace

Style::Style(const pugi::xml_node node) : m_node{node} {
  m_resolved.text_style = TextStyle();
  m_resolved.text_style.font_size = Measure(12, DynamicUnit("pt"));

  resolve_default_style_();
}

Style::Style(std::string name, const pugi::xml_node node, const Style *parent)
    : m_name{std::move(name)}, m_node{node}, m_parent{parent} {
  if (parent != nullptr) {
    m_resolved = parent->m_resolved;
    m_contextual_spacing = parent->m_contextual_spacing;
  }

  resolve_style_();
}

std::string Style::name() const { return m_name; }

const Style *Style::parent() const { return m_parent; }

const ResolvedStyle &Style::resolved() const { return m_resolved; }

bool Style::contextual_spacing() const { return m_contextual_spacing; }

void Style::resolve_style_() {
  if (const pugi::xml_node contextual_spacing =
          m_node.child("w:pPr").child("w:contextualSpacing")) {
    m_contextual_spacing = read_on_off_attribute(contextual_spacing);
  }

  resolve_text_style_(m_node, m_resolved.text_style);
  resolve_paragraph_style_(m_node, m_resolved.paragraph_style);
  resolve_table_style_(m_node, m_resolved.table_style);
  resolve_table_row_style_(m_node, m_resolved.table_row_style);
  resolve_table_cell_style_(m_node, m_resolved.table_cell_style);
  resolve_graphic_style_(m_node, m_resolved.graphic_style);
}

void Style::resolve_default_style_() {
  resolve_text_style_(m_node.child("w:rPrDefault"), m_resolved.text_style);
  resolve_paragraph_style_(m_node.child("w:pPrDefault"),
                           m_resolved.paragraph_style);
  resolve_table_style_(m_node.child("w:tblPrDefault"), m_resolved.table_style);
  resolve_table_row_style_(m_node.child("w:trPrDefault"),
                           m_resolved.table_row_style);
  resolve_table_cell_style_(m_node.child("w:tcPrDefault"),
                            m_resolved.table_cell_style);
}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_(styles_root);
}

Style *StyleRegistry::default_style() const { return m_default_style.get(); }

Style *StyleRegistry::style(const std::string &name) const {
  if (const auto styles_it = m_styles.find(name);
      styles_it != std::end(m_styles)) {
    return styles_it->second.get();
  }
  return nullptr;
}

ResolvedStyle
StyleRegistry::partial_text_style(const pugi::xml_node node) const {
  ResolvedStyle result;
  // TODO consider w:default="1"
  if (const pugi::xml_attribute style_name =
          node.child("w:rPr").child("w:rStyle").attribute("w:val")) {
    if (const Style *style = this->style(style_name.value())) {
      result.text_style = style->resolved().text_style;
    }
  }
  resolve_text_style_(node, result.text_style);
  return result;
}

ResolvedStyle
StyleRegistry::partial_paragraph_style(const pugi::xml_node node) const {
  const pugi::xml_node paragraph_properties = node.child("w:pPr");
  const pugi::xml_attribute style_name =
      paragraph_properties.child("w:pStyle").attribute("w:val");

  ResolvedStyle result;
  // TODO consider w:default="1"
  const Style *style = style_name ? this->style(style_name.value()) : nullptr;
  if (style != nullptr) {
    result = style->resolved();
  }
  resolve_paragraph_style_(node, result.paragraph_style);
  result.override(partial_text_style(paragraph_properties));

  bool contextual_spacing = style != nullptr && style->contextual_spacing();
  if (const pugi::xml_node contextual_spacing_node =
          paragraph_properties.child("w:contextualSpacing")) {
    contextual_spacing = read_on_off_attribute(contextual_spacing_node);
  }
  if (contextual_spacing) {
    // [ECMA-376] 17.3.1.9: the spacing towards a neighbouring paragraph of the
    // same style is dropped, which is what keeps a list tight
    const Measure none(0, DynamicUnit("in"));
    if (has_paragraph_style(node.previous_sibling(), style_name)) {
      result.paragraph_style.margin.top = none;
    }
    if (has_paragraph_style(node.next_sibling(), style_name)) {
      result.paragraph_style.margin.bottom = none;
    }
  }
  return result;
}

ResolvedStyle
StyleRegistry::partial_table_style(const pugi::xml_node node) const {
  ResolvedStyle result;
  // a table style also carries the paragraph and text properties of everything
  // in the table, which the element tree cascades down from here
  if (const pugi::xml_attribute style_name =
          node.child("w:tblPr").child("w:tblStyle").attribute("w:val")) {
    if (const Style *style = this->style(style_name.value())) {
      result = style->resolved();
    }
  }
  resolve_table_style_(node, result.table_style);
  return result;
}

ResolvedStyle
StyleRegistry::partial_table_row_style(const pugi::xml_node node) const {
  ResolvedStyle result;
  resolve_table_row_style_(node, result.table_row_style);
  return result;
}

ResolvedStyle
StyleRegistry::partial_table_cell_style(const pugi::xml_node node) const {
  ResolvedStyle result;
  resolve_table_cell_style_(node, result.table_cell_style);
  return result;
}

void StyleRegistry::generate_indices_(const pugi::xml_node styles_root) {
  for (const pugi::xml_node style : styles_root) {
    const std::string element_name = style.name();

    if (element_name == "w:style") {
      m_index[style.attribute("w:styleId").value()] = style;
    }
  }
}

void StyleRegistry::generate_styles_(const pugi::xml_node styles_root) {
  m_default_style = std::make_unique<Style>(styles_root.child("w:docDefaults"));

  for (const auto &[name, node] : m_index) {
    generate_style_(name, node);
  }
}

/// Walks the `w:basedOn` chain onto a stack and builds it from the root down;
/// recursing it costs a stack frame per link.
Style *StyleRegistry::generate_style_(const std::string &name,
                                      const pugi::xml_node node) {
  // the names are the map's own, which keep their addresses across a rehash
  std::vector<std::pair<const std::string *, pugi::xml_node>> chain;

  const std::string *current_name = &name;
  pugi::xml_node current_node = node;
  Style *parent{nullptr};

  while (true) {
    // an entry present but still null means the link is already on this chain
    const auto [styles_it, inserted] = m_styles.try_emplace(*current_name);
    if (!inserted) {
      parent = styles_it->second.get();
      break;
    }
    chain.emplace_back(&styles_it->first, current_node);

    const pugi::xml_attribute parent_attr =
        current_node.child("w:basedOn").attribute("w:val");
    if (!parent_attr) {
      break;
    }
    // `find`, not `operator[]`: an unknown parent id must not grow m_index
    // while generate_styles_ iterates it
    const auto index_it = m_index.find(parent_attr.value());
    if (index_it == std::end(m_index)) {
      break;
    }
    current_name = &index_it->first;
    current_node = index_it->second;
  }

  for (const auto &[chain_name, chain_node] : chain | std::views::reverse) {
    std::unique_ptr<Style> &style = m_styles[*chain_name];
    style = std::make_unique<Style>(*chain_name, chain_node, parent);
    parent = style.get();
  }

  return parent;
}

} // namespace odr::internal::ooxml::text
