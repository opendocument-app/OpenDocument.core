#include <odr/internal/odf/odf_style.hpp>

#include <odr/internal/odf/odf_parser.hpp>

#include <cstring>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

namespace {

std::optional<std::string> read_string(const pugi::xml_attribute attribute) {
  if (attribute) {
    return attribute.value();
  }
  return {};
}

std::optional<Measure> read_measure(const pugi::xml_attribute attribute) {
  if (attribute) {
    try {
      return Measure(attribute.value());
    } catch (...) {
      // TODO log
    }
  }
  return {};
}

std::optional<FontWeight>
read_font_weight(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("normal", value) == 0) {
    return FontWeight::normal;
  }
  if (std::strcmp("bold", value) == 0) {
    return FontWeight::bold;
  }
  return {};
}

std::optional<FontStyle> read_font_style(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("normal", value) == 0) {
    return FontStyle::normal;
  }
  if (std::strcmp("italic", value) == 0) {
    return FontStyle::italic;
  }
  return {};
}

std::optional<TextAlign> read_text_align(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("left", value) == 0 || std::strcmp("start", value) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("right", value) == 0 || std::strcmp("end", value) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("center", value) == 0) {
    return TextAlign::center;
  }
  if (std::strcmp("justify", value) == 0) {
    return TextAlign::justify;
  }
  return {};
}

std::optional<VerticalAlign>
read_vertical_align(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("top", value) == 0) {
    return VerticalAlign::top;
  }
  if (std::strcmp("middle", value) == 0) {
    return VerticalAlign::middle;
  }
  if (std::strcmp("bottom", value) == 0) {
    return VerticalAlign::bottom;
  }
  return {};
}

std::optional<TextWrap> read_text_wrap(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("none", value) == 0) {
    return TextWrap::none;
  }
  if (std::strcmp("biggest", value) == 0 ||
      std::strcmp("dynamic", value) == 0 || std::strcmp("left", value) == 0 ||
      std::strcmp("parallel", value) == 0) {
    return TextWrap::before;
  }
  if (std::strcmp("right", value) == 0) {
    return TextWrap::after;
  }
  if (std::strcmp("run-through", value) == 0) {
    return TextWrap::run_through;
  }
  return {};
}

std::optional<PrintOrientation>
read_print_orientation(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("portrait", value) == 0) {
    return PrintOrientation::portrait;
  }
  if (std::strcmp("landscape", value) == 0) {
    return PrintOrientation::landscape;
  }
  return {};
}

std::optional<Color> read_color(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  const char *value = attribute.value();
  if (std::strcmp("transparent", attribute.value()) == 0) {
    return {}; // TODO use alpha
  }
  if (value[0] == '#') {
    const std::uint32_t color = std::strtoull(&value[1], nullptr, 16);
    return Color(static_cast<std::uint8_t>(color >> 16),
                 static_cast<std::uint8_t>(color >> 8),
                 static_cast<std::uint8_t>(color >> 0));
  }
  // TODO log
  // throw std::invalid_argument("# missing");
  return {};
}

std::optional<std::string> read_border(const pugi::xml_attribute attribute) {
  if (attribute && std::strcmp("none", attribute.value()) != 0) {
    return attribute.value();
  }
  return {};
}

PageLayout read_page_layout(const pugi::xml_node node) {
  PageLayout result;

  const pugi::xml_node page_layout_properties =
      node.child("style:page-layout-properties");

  result.width =
      read_measure(page_layout_properties.attribute("fo:page-width"));
  result.height =
      read_measure(page_layout_properties.attribute("fo:page-height"));
  result.print_orientation = read_print_orientation(
      page_layout_properties.attribute("style:print-orientation"));
  result.margin = DirectionalStyle(
      read_measure(page_layout_properties.attribute("fo:margin")));
  if (const pugi::xml_attribute margin_right =
          page_layout_properties.attribute("fo:margin-right")) {
    result.margin.right = read_measure(margin_right);
  }
  if (const pugi::xml_attribute margin_top =
          page_layout_properties.attribute("fo:margin-top")) {
    result.margin.top = read_measure(margin_top);
  }
  if (const pugi::xml_attribute margin_left =
          page_layout_properties.attribute("fo:margin-left")) {
    result.margin.left = read_measure(margin_left);
  }
  if (const pugi::xml_attribute margin_bottom =
          page_layout_properties.attribute("fo:margin-bottom")) {
    result.margin.bottom = read_measure(margin_bottom);
  }

  return result;
}

void read_font_face(const char *name, const pugi::xml_node node,
                    TextStyle &result) {
  if (!node) {
    result.font_name = name;
    return;
  }

  result.font_name = node.attribute("svg:font-family").value();
}

} // namespace

Style::Style() = default;

Style::Style(const StyleRegistry *registry, std::string family,
             const pugi::xml_node node)
    : m_registry{registry}, m_name{std::move(family)}, m_node{node} {
  resolve_style_();
}

Style::Style(const StyleRegistry *registry, std::string name,
             const pugi::xml_node node, Style *parent, Style *family)
    : m_registry{registry}, m_name{std::move(name)}, m_node{node},
      m_parent{parent}, m_family{family} {
  if (const Style *copy_from = m_parent != nullptr ? m_parent : m_family;
      copy_from != nullptr) {
    m_resolved = copy_from->m_resolved;
  }

  resolve_style_();
}

std::string Style::name() const { return m_name; }

const ResolvedStyle &Style::resolved() const { return m_resolved; }

void Style::resolve_style_() {
  // TODO use override?
  resolve_text_style_(m_registry, m_node, m_resolved.text_style);
  resolve_paragraph_style_(m_node, m_resolved.paragraph_style);
  resolve_table_style_(m_node, m_resolved.table_style);
  resolve_table_column_style_(m_node, m_resolved.table_column_style);
  resolve_table_row_style_(m_node, m_resolved.table_row_style);
  resolve_table_cell_style_(m_node, m_resolved.table_cell_style);
  resolve_graphic_style_(m_node, m_resolved.graphic_style);
}

void Style::resolve_text_style_(const StyleRegistry *registry,
                                const pugi::xml_node node, TextStyle &result) {
  const pugi::xml_node text_properties = node.child("style:text-properties");

  if (const pugi::xml_attribute font_name =
          text_properties.attribute("style:font-name")) {
    read_font_face(font_name.value(),
                   registry->font_face_node(font_name.value()), result);
  }
  if (const std::optional<Measure> font_size =
          read_measure(text_properties.attribute("fo:font-size"))) {
    // TODO
    if (font_size->unit().name() != "%") {
      result.font_size = font_size;
    } else {
      if (const std::optional<Measure> parent_font_size = result.font_size) {
        result.font_size = Measure(parent_font_size->magnitude() *
                                       font_size->magnitude() * 1e-2,
                                   parent_font_size->unit());
      }
    }
  }
  if (const std::optional<FontWeight> font_weight =
          read_font_weight(text_properties.attribute("fo:font-weight"))) {
    result.font_weight = font_weight;
  }
  if (const std::optional<FontStyle> font_style =
          read_font_style(text_properties.attribute("fo:font-style"))) {
    result.font_style = font_style;
  }
  if (const pugi::xml_attribute font_underline =
          text_properties.attribute("style:text-underline-style");
      font_underline) {
    result.font_underline = std::strcmp("none", font_underline.value()) != 0;
  }
  if (const pugi::xml_attribute font_line_through =
          text_properties.attribute("style:text-line-through-style");
      font_line_through) {
    result.font_line_through =
        std::strcmp("none", font_line_through.value()) != 0;
  }
  if (const std::optional<std::string> font_shadow =
          read_string(text_properties.attribute("fo:text-shadow"))) {
    result.font_shadow = font_shadow;
  }
  if (const std::optional<Color> font_color =
          read_color(text_properties.attribute("fo:color"))) {
    result.font_color = font_color;
  }
  if (const std::optional<Color> background_color =
          read_color(text_properties.attribute("fo:background-color"))) {
    result.background_color = background_color;
  }
}

void Style::resolve_paragraph_style_(const pugi::xml_node node,
                                     ParagraphStyle &result) {
  const pugi::xml_node paragraph_properties =
      node.child("style:paragraph-properties");

  if (const std::optional<TextAlign> text_align =
          read_text_align(paragraph_properties.attribute("fo:text-align"))) {
    result.text_align = text_align;
  }
  if (const std::optional<Measure> margin =
          read_measure(paragraph_properties.attribute("fo:margin"))) {
    // TODO
    if (margin->unit().name() != "%") {
      result.margin = DirectionalStyle(margin);
    }
  }
  if (const std::optional<Measure> margin_right =
          read_measure(paragraph_properties.attribute("fo:margin-right"))) {
    // TODO
    if (margin_right->unit().name() != "%") {
      result.margin.right = margin_right;
    }
  }
  if (const std::optional<Measure> margin_top =
          read_measure(paragraph_properties.attribute("fo:margin-top"))) {
    // TODO
    if (margin_top->unit().name() != "%") {
      result.margin.top = margin_top;
    }
  }
  if (const std::optional<Measure> margin_left =
          read_measure(paragraph_properties.attribute("fo:margin-left"))) {
    // TODO
    if (margin_left->unit().name() != "%") {
      result.margin.left = margin_left;
    }
  }
  if (const std::optional<Measure> margin_bottom =
          read_measure(paragraph_properties.attribute("fo:margin-bottom"))) {
    // TODO
    if (margin_bottom->unit().name() != "%") {
      result.margin.bottom = margin_bottom;
    }
  }
  if (const std::optional<Measure> line_height =
          read_measure(paragraph_properties.attribute("fo:line-height"))) {
    // TODO
    if (line_height->unit().name() != "%") {
      result.line_height = line_height;
    }
  }
}

void Style::resolve_table_style_(const pugi::xml_node node,
                                 TableStyle &result) {
  const pugi::xml_node table_properties = node.child("style:table-properties");

  if (const std::optional<Measure> width =
          read_measure(table_properties.attribute("style:width"))) {
    result.width = width;
  }
}

void Style::resolve_table_column_style_(const pugi::xml_node node,
                                        TableColumnStyle &result) {
  const pugi::xml_node table_column_properties =
      node.child("style:table-column-properties");

  if (const std::optional<Measure> width = read_measure(
          table_column_properties.attribute("style:column-width"))) {
    result.width = width;
  }
}

void Style::resolve_table_row_style_(const pugi::xml_node node,
                                     TableRowStyle &result) {
  const pugi::xml_node table_row_properties =
      node.child("style:table-row-properties");

  if (const std::optional<Measure> height =
          read_measure(table_row_properties.attribute("style:row-height"))) {
    result.height = height;
  }
}

void Style::resolve_table_cell_style_(const pugi::xml_node node,
                                      TableCellStyle &result) {
  const pugi::xml_node table_cell_properties =
      node.child("style:table-cell-properties");

  if (const std::optional<VerticalAlign> vertical_align = read_vertical_align(
          table_cell_properties.attribute("style:vertical-align"))) {
    result.vertical_align = vertical_align;
  }
  if (const std::optional<Color> background_color =
          read_color(table_cell_properties.attribute("fo:background-color"))) {
    result.background_color = background_color;
  }
  if (const std::optional<Measure> padding =
          read_measure(table_cell_properties.attribute("fo:padding"))) {
    result.padding = DirectionalStyle(padding);
  }
  if (const std::optional<Measure> padding_right =
          read_measure(table_cell_properties.attribute("fo:padding-right"))) {
    result.padding.right = padding_right;
  }
  if (const std::optional<Measure> padding_top =
          read_measure(table_cell_properties.attribute("fo:padding-top"))) {
    result.padding.top = padding_top;
  }
  if (const std::optional<Measure> padding_left =
          read_measure(table_cell_properties.attribute("fo:padding-left"))) {
    result.padding.left = padding_left;
  }
  if (const std::optional<Measure> padding_bottom =
          read_measure(table_cell_properties.attribute("fo:padding-bottom"))) {
    result.padding.bottom = padding_bottom;
  }
  if (const std::optional<std::string> border =
          read_border(table_cell_properties.attribute("fo:border"))) {
    result.border = DirectionalStyle(border);
  }
  if (const std::optional<std::string> border_right =
          read_border(table_cell_properties.attribute("fo:border-right"))) {
    result.border.right = border_right;
  }
  if (const std::optional<std::string> border_top =
          read_border(table_cell_properties.attribute("fo:border-top"))) {
    result.border.top = border_top;
  }
  if (const std::optional<std::string> border_left =
          read_border(table_cell_properties.attribute("fo:border-left"))) {
    result.border.left = border_left;
  }
  if (const std::optional<std::string> border_bottom =
          read_border(table_cell_properties.attribute("fo:border-bottom"))) {
    result.border.bottom = border_bottom;
  }
}

void Style::resolve_graphic_style_(const pugi::xml_node node,
                                   GraphicStyle &result) {
  const pugi::xml_node graphic_properties =
      node.child("style:graphic-properties");

  if (const std::optional<Measure> stroke_width =
          read_measure(graphic_properties.attribute("svg:stroke-width"))) {
    result.stroke_width = stroke_width;
  }
  if (const std::optional<Color> stroke_color =
          read_color(graphic_properties.attribute("svg:stroke-color"))) {
    result.stroke_color = stroke_color;
  }
  if (const std::optional<Color> fill_color =
          read_color(graphic_properties.attribute("draw:fill-color"))) {
    result.fill_color = fill_color;
  }
  if (const std::optional<VerticalAlign> vertical_align = read_vertical_align(
          graphic_properties.attribute("draw:textarea-vertical-align"))) {
    result.vertical_align = vertical_align;
  }
  if (const std::optional<TextWrap> text_wrap =
          read_text_wrap(graphic_properties.attribute("style:wrap"))) {
    result.text_wrap = text_wrap;
  }
}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(Document &document,
                             const pugi::xml_node content_root,
                             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
  generate_master_pages_(document);
}

void StyleRegistry::generate_indices_(const pugi::xml_node content_root,
                                      const pugi::xml_node styles_root) {
  generate_indices_(styles_root.child("office:font-face-decls"));
  generate_indices_(styles_root.child("office:styles"));
  generate_indices_(styles_root.child("office:automatic-styles"));
  generate_indices_(styles_root.child("office:master-styles"));

  // content styles
  generate_indices_(content_root.child("office:font-face-decls"));
  generate_indices_(content_root.child("office:automatic-styles"));
}

void StyleRegistry::generate_indices_(const pugi::xml_node node) {
  for (const pugi::xml_node e : node) {
    if (const std::string name = e.name(); name == "style:font-face") {
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

void StyleRegistry::generate_styles_() {
  for (const auto &[name, node] : m_index_default_style) {
    generate_default_style_(name, node);
  }

  for (const auto &[name, node] : m_index_style) {
    generate_style_(name, node);
  }
}

Style *StyleRegistry::generate_default_style_(const std::string &name,
                                              const pugi::xml_node node) {
  std::unique_ptr<Style> &style = m_default_styles[name];
  if (style) {
    return style.get();
  }
  style = std::make_unique<Style>(this, name, node);
  return style.get();
}

Style *StyleRegistry::generate_style_(const std::string &name,
                                      const pugi::xml_node node) {
  auto &&style = m_styles[name];
  if (style != nullptr) {
    return style.get();
  }

  Style *parent{nullptr};
  if (const pugi::xml_attribute parent_attr =
          node.attribute("style:parent-style-name");
      parent_attr) {
    if (const pugi::xml_node parent_node = m_index_style[parent_attr.value()]) {
      parent = generate_style_(parent_attr.value(), parent_node);
    }
  }

  Style *family{nullptr};
  if (const pugi::xml_attribute family_attr = node.attribute("style:family");
      family_attr) {
    family = generate_default_style_(family_attr.value(), {});
  }

  style = std::make_unique<Style>(this, name, node, parent, family);
  return style.get();
}

void StyleRegistry::generate_master_pages_(Document &document) {
  for (const auto &[name, node] : m_index_master_page) {
    m_master_page_elements[name] =
        dynamic_cast<MasterPage *>(parse_tree(document, node));
  }

  if (m_first_master_page) {
    m_first_master_page_element = m_master_page_elements[*m_first_master_page];
  }
}

Style *StyleRegistry::style(const char *name) const {
  if (const auto style_it = m_styles.find(name);
      style_it != std::end(m_styles)) {
    return style_it->second.get();
  }
  return {};
}

PageLayout StyleRegistry::page_layout(const std::string &name) const {
  if (const auto page_layout_it = m_index_page_layout.find(name);
      page_layout_it != std::end(m_index_page_layout)) {
    return read_page_layout(page_layout_it->second);
  }
  return {};
}

pugi::xml_node StyleRegistry::font_face_node(const std::string &name) const {
  if (const auto font_face_it = m_index_font_face.find(name);
      font_face_it != std::end(m_index_font_face)) {
    return font_face_it->second;
  }
  return {};
}

MasterPage *StyleRegistry::master_page(const std::string &name) const {
  if (const auto master_page_elements_it = m_master_page_elements.find(name);
      master_page_elements_it != std::end(m_master_page_elements)) {
    return master_page_elements_it->second;
  }
  return {};
}

MasterPage *StyleRegistry::first_master_page() const {
  return m_first_master_page_element;
}

} // namespace odr::internal::odf
