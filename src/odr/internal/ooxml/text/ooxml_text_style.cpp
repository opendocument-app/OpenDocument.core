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
  if (const std::optional<TextDirection> direction =
          read_text_direction_attribute(paragraph_properties.child("w:bidi"))) {
    result.direction = direction;
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
  // [ECMA-376] 17.3.1.23. `w:val="0"` clears an inherited break, so off has to
  // be told from silence.
  if (const pugi::xml_node page_break_before =
          paragraph_properties.child("w:pageBreakBefore")) {
    result.break_before = read_on_off_attribute(page_break_before)
                              ? BreakType::page
                              : BreakType::none;
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

  // [ECMA-376] 17.4.39
  const pugi::xml_node borders = table_properties.child("w:tblBorders");
  result.border.override(read_borders_node(borders));
  if (const std::optional<std::string> inside_horizontal =
          read_border_node(borders.child("w:insideH"))) {
    result.border_inside_horizontal = inside_horizontal;
  }
  if (const std::optional<std::string> inside_vertical =
          read_border_node(borders.child("w:insideV"))) {
    result.border_inside_vertical = inside_vertical;
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
  result.border.override(
      read_borders_node(table_cell_properties.child("w:tcBorders")));
}

std::optional<std::string> read_cell_border(const pugi::xml_node cell,
                                            const char *side) {
  return read_border_node(
      cell.child("w:tcPr").child("w:tcBorders").child(side));
}

/// A cell's edge: its own, then the neighbour's opposite edge, then the
/// table's.
std::optional<std::string>
resolve_cell_border(std::optional<std::string> own,
                    std::optional<std::string> neighbour,
                    std::optional<std::string> table) {
  if (own.has_value()) {
    return own;
  }
  return neighbour.has_value() ? std::move(neighbour) : std::move(table);
}

/// [ECMA-376] 20.4.2.*. `wrapText` names the side the text keeps; where it
/// names both, css has to pick one, and no float holds a centred frame.
std::optional<TextWrap>
read_frame_text_wrap(const pugi::xml_node anchor,
                     const std::optional<HorizontalAlign> position) {
  if (anchor.child("wp:wrapNone")) {
    return TextWrap::run_through;
  }
  if (anchor.child("wp:wrapTopAndBottom")) {
    return TextWrap::none;
  }
  for (const char *name : {"wp:wrapSquare", "wp:wrapTight", "wp:wrapThrough"}) {
    const pugi::xml_node wrap = anchor.child(name);
    if (!wrap) {
      continue;
    }
    const char *wrap_text = wrap.attribute("wrapText").value();
    if (std::strcmp("left", wrap_text) == 0) {
      return TextWrap::before;
    }
    if (std::strcmp("right", wrap_text) == 0) {
      return TextWrap::after;
    }
    if (position == HorizontalAlign::center) {
      return TextWrap::none;
    }
    return position == HorizontalAlign::left ? TextWrap::after
                                             : TextWrap::before;
  }
  return {};
}

/// [ECMA-376] 20.4.3.1 `wp:align`, the side a frame takes instead of an offset.
std::optional<HorizontalAlign>
read_frame_horizontal_position(const pugi::xml_node align) {
  if (!align) {
    return {};
  }
  const char *value = align.text().get();
  if (std::strcmp("left", value) == 0 || std::strcmp("inside", value) == 0) {
    return HorizontalAlign::left;
  }
  if (std::strcmp("center", value) == 0) {
    return HorizontalAlign::center;
  }
  if (std::strcmp("right", value) == 0 || std::strcmp("outside", value) == 0) {
    return HorizontalAlign::right;
  }
  return {};
}

/// A `w:sdt` and its `w:sdtContent` become a group, which the html renderer
/// writes as nothing but its children — the paragraphs around one are
/// neighbours on the page.
bool is_transparent_wrapper(const pugi::xml_node node) {
  const char *name = node.name();
  return std::strcmp("w:sdt", name) == 0 ||
         std::strcmp("w:sdtContent", name) == 0;
}

bool is_block(const pugi::xml_node node) {
  const char *name = node.name();
  return std::strcmp("w:p", name) == 0 || std::strcmp("w:tbl", name) == 0;
}

/// The block `node` puts on the given side, `node` itself unless it wraps one;
/// nothing for a marker element such as `w:bookmarkEnd`.
pugi::xml_node block_within(const pugi::xml_node node, const bool previous) {
  if (!is_transparent_wrapper(node)) {
    return is_block(node) ? node : pugi::xml_node();
  }
  for (pugi::xml_node child = previous ? node.last_child() : node.first_child();
       child;
       child = previous ? child.previous_sibling() : child.next_sibling()) {
    if (const pugi::xml_node block = block_within(child, previous)) {
      return block;
    }
  }
  return {};
}

/// The block that neighbours `node` in document order, stepping over marker
/// elements and seeing through the wrappers. Stops at anything else — a cell
/// or the body end — so a paragraph never neighbours one outside its container.
pugi::xml_node block_neighbour(pugi::xml_node node, const bool previous) {
  while (true) {
    for (pugi::xml_node sibling = previous ? node.previous_sibling()
                                           : node.next_sibling();
         sibling; sibling = previous ? sibling.previous_sibling()
                                     : sibling.next_sibling()) {
      if (const pugi::xml_node block = block_within(sibling, previous)) {
        return block;
      }
    }
    if (!is_transparent_wrapper(node.parent())) {
      return {};
    }
    node = node.parent();
  }
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
    if (has_paragraph_style(block_neighbour(node, true), style_name)) {
      result.paragraph_style.margin.top = none;
    }
    if (has_paragraph_style(block_neighbour(node, false), style_name)) {
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

namespace odr::internal::ooxml {

DirectionalStyle<std::string> text::table_cell_border(
    const pugi::xml_node node, const pugi::xml_node cell_above,
    const TableStyle &table_style, const std::uint32_t rows) {
  const pugi::xml_node row_node = node.parent();
  const pugi::xml_node cell_before = node.previous_sibling("w:tc");

  DirectionalStyle<std::string> result;
  result.top = resolve_cell_border(
      read_cell_border(node, "w:top"), read_cell_border(cell_above, "w:bottom"),
      row_node.previous_sibling("w:tr") ? table_style.border_inside_horizontal
                                        : table_style.border.top);
  result.left =
      resolve_cell_border(read_cell_border(node, "w:left"),
                          read_cell_border(cell_before, "w:right"),
                          cell_before ? table_style.border_inside_vertical
                                      : table_style.border.left);
  if (!node.next_sibling("w:tc")) {
    result.right = resolve_cell_border(read_cell_border(node, "w:right"), {},
                                       table_style.border.right);
  }
  // a merged cell reaches down to where its continuations end
  pugi::xml_node last_row_node = row_node;
  for (std::uint32_t row = 1; row < rows; ++row) {
    const pugi::xml_node next_row_node = last_row_node.next_sibling("w:tr");
    if (!next_row_node) {
      break;
    }
    last_row_node = next_row_node;
  }
  if (!last_row_node.next_sibling("w:tr")) {
    result.bottom = resolve_cell_border(read_cell_border(node, "w:bottom"), {},
                                        table_style.border.bottom);
  }
  return result;
}

GraphicStyle text::read_frame_style(const pugi::xml_node inner_node) {
  GraphicStyle result;
  result.horizontal_position = read_frame_horizontal_position(
      inner_node.child("wp:positionH").child("wp:align"));
  result.text_wrap =
      read_frame_text_wrap(inner_node, result.horizontal_position);
  return result;
}

std::optional<Measure> text::read_frame_offset(const pugi::xml_node position) {
  if (std::strcmp("page", position.attribute("relativeFrom").value()) == 0) {
    return {};
  }
  return read_emus_text(position.child("wp:posOffset"));
}

} // namespace odr::internal::ooxml
