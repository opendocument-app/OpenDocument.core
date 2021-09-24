#include <cstdint>
#include <cstring>
#include <internal/common/style.h>
#include <internal/odf/odf_style.h>
#include <iterator>
#include <odr/quantity.h>
#include <odr/style.h>
#include <pugixml.hpp>
#include <stdlib.h>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

namespace {

std::optional<std::string> read_string(pugi::xml_attribute attribute) {
  if (attribute) {
    return attribute.value();
  }
  return {};
}

std::optional<Measure> read_measure(pugi::xml_attribute attribute) {
  if (attribute) {
    try {
      return Measure(attribute.value());
    } catch (...) {
      // TODO log
    }
  }
  return {};
}

std::optional<FontWeight> read_font_weight(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("normal", value) == 0) {
    return FontWeight::normal;
  }
  if (std::strcmp("bold", value) == 0) {
    return FontWeight::bold;
  }
  return {};
}

std::optional<FontStyle> read_font_style(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("normal", value) == 0) {
    return FontStyle::normal;
  }
  if (std::strcmp("italic", value) == 0) {
    return FontStyle::italic;
  }
  return {};
}

std::optional<TextAlign> read_text_align(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if ((std::strcmp("left", value) == 0) || (std::strcmp("start", value) == 0)) {
    return TextAlign::left;
  }
  if ((std::strcmp("right", value) == 0) || (std::strcmp("end", value) == 0)) {
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
read_vertical_align(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
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

bool read_text_wrap(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if ((std::strcmp("biggest", value) == 0) ||
      (std::strcmp("dynamic", value) == 0) ||
      (std::strcmp("left", value) == 0) ||
      (std::strcmp("parallel", value) == 0) ||
      (std::strcmp("right", value) == 0)) {
    return true;
  }
  if ((std::strcmp("none", value) == 0) ||
      (std::strcmp("run-through", value) == 0)) {
    return false;
  }
  return false;
}

std::optional<PrintOrientation>
read_print_orientation(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("portrait", value) == 0) {
    return PrintOrientation::portrait;
  }
  if (std::strcmp("landscape", value) == 0) {
    return PrintOrientation::landscape;
  }
  return {};
}

std::optional<Color> read_color(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("transparent", attribute.value()) == 0) {
    return {}; // TODO use alpha
  }
  if (value[0] == '#') {
    std::uint32_t color = std::strtoull(&value[1], nullptr, 16);
    return Color((std::uint8_t)(color >> 16), (std::uint8_t)(color >> 8),
                 (std::uint8_t)(color >> 0));
  }
  // TODO log
  // throw std::invalid_argument("# missing");
  return {};
}

std::optional<std::string> read_border(pugi::xml_attribute attribute) {
  if (attribute && (std::strcmp("none", attribute.value()) != 0)) {
    return attribute.value();
  }
  return {};
}

PageLayout read_page_layout(pugi::xml_node node) {
  PageLayout result;

  auto page_layout_properties = node.child("style:page-layout-properties");

  result.width =
      read_measure(page_layout_properties.attribute("fo:page-width"));
  result.height =
      read_measure(page_layout_properties.attribute("fo:page-height"));
  result.print_orientation = read_print_orientation(
      page_layout_properties.attribute("style:print-orientation"));
  result.margin = read_measure(page_layout_properties.attribute("fo:margin"));
  if (auto margin_right = page_layout_properties.attribute("fo:margin-right")) {
    result.margin.right = read_measure(margin_right);
  }
  if (auto margin_top = page_layout_properties.attribute("fo:margin-top")) {
    result.margin.top = read_measure(margin_top);
  }
  if (auto margin_left = page_layout_properties.attribute("fo:margin-left")) {
    result.margin.left = read_measure(margin_left);
  }
  if (auto margin_bottom =
          page_layout_properties.attribute("fo:margin-bottom")) {
    result.margin.bottom = read_measure(margin_bottom);
  }

  return result;
}

void read_font_face(const char *name, pugi::xml_node node, TextStyle &result) {
  if (!node) {
    result.font_name = name;
    return;
  }

  result.font_name = node.attribute("svg:font-family").value();
}

} // namespace

Style::Style() = default;

Style::Style(const StyleRegistry *registry, std::string family,
             pugi::xml_node node)
    : m_registry{registry}, m_name{std::move(family)}, m_node{node} {
  resolve_style_();
}

Style::Style(const StyleRegistry *registry, std::string name,
             pugi::xml_node node, Style *parent, Style *family)
    : m_registry{registry}, m_name{std::move(name)}, m_node{node},
      m_parent{parent}, m_family{family} {
  if (Style *copy_from = m_parent ? m_parent : m_family) {
    m_resolved = copy_from->m_resolved;
  }

  resolve_style_();
}

std::string Style::name() const { return m_name; }

const common::ResolvedStyle &Style::resolved() const { return m_resolved; }

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
                                pugi::xml_node node,
                                std::optional<TextStyle> &result) {
  if (auto text_properties = node.child("style:text-properties")) {
    if (!result) {
      result = TextStyle();
    }

    if (auto font_name = text_properties.attribute("style:font-name")) {
      read_font_face(font_name.value(),
                     registry->font_face_node(font_name.value()), *result);
    }
    if (auto font_size =
            read_measure(text_properties.attribute("fo:font-size"))) {
      result->font_size = font_size;
    }
    if (auto font_weight =
            read_font_weight(text_properties.attribute("fo:font-weight"))) {
      result->font_weight = font_weight;
    }
    if (auto font_style =
            read_font_style(text_properties.attribute("fo:font-style"))) {
      result->font_style = font_style;
    }
    if (auto font_underline =
            text_properties.attribute("style:text-underline-style");
        font_underline && (std::strcmp("none", font_underline.value()) != 0)) {
      result->font_underline = true;
    }
    if (auto font_line_through =
            text_properties.attribute("style:text-line-through-style");
        font_line_through &&
        (std::strcmp("none", font_line_through.value()) != 0)) {
      result->font_line_through = true;
    }
    if (auto font_shadow =
            read_string(text_properties.attribute("fo:text-shadow"))) {
      result->font_shadow = font_shadow;
    }
    if (auto font_color = read_color(text_properties.attribute("fo:color"))) {
      result->font_color = font_color;
    }
    if (auto background_color =
            read_color(text_properties.attribute("fo:background-color"))) {
      result->background_color = background_color;
    }
  }
}

void Style::resolve_paragraph_style_(pugi::xml_node node,
                                     std::optional<ParagraphStyle> &result) {
  if (auto paragraph_properties = node.child("style:paragraph-properties")) {
    if (!result) {
      result = ParagraphStyle();
    }

    if (auto text_align =
            read_text_align(paragraph_properties.attribute("fo:text-align"))) {
      result->text_align = text_align;
    }
    if (auto margin =
            read_measure(paragraph_properties.attribute("fo:margin"))) {
      result->margin = margin;
    }
    if (auto margin_right =
            read_measure(paragraph_properties.attribute("fo:margin-right"))) {
      result->margin.right = margin_right;
    }
    if (auto margin_top =
            read_measure(paragraph_properties.attribute("fo:margin-top"))) {
      result->margin.top = margin_top;
    }
    if (auto margin_left =
            read_measure(paragraph_properties.attribute("fo:margin-left"))) {
      result->margin.left = margin_left;
    }
    if (auto margin_bottom =
            read_measure(paragraph_properties.attribute("fo:margin-bottom"))) {
      result->margin.bottom = margin_bottom;
    }
  }
}

void Style::resolve_table_style_(pugi::xml_node node,
                                 std::optional<TableStyle> &result) {
  if (auto table_properties = node.child("style:table-properties")) {
    if (!result) {
      result = TableStyle();
    }

    if (auto width = read_measure(table_properties.attribute("style:width"))) {
      result->width = width;
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

    if (auto width = read_measure(
            table_column_properties.attribute("style:column-width"))) {
      result->width = width;
    }
  }
}

void Style::resolve_table_row_style_(pugi::xml_node node,
                                     std::optional<TableRowStyle> &result) {
  if (auto table_row_properties = node.child("style:table-row-properties")) {
    if (!result) {
      result = TableRowStyle();
    }

    if (auto height =
            read_measure(table_row_properties.attribute("style:row-height"))) {
      result->height = height;
    }
  }
}

void Style::resolve_table_cell_style_(pugi::xml_node node,
                                      std::optional<TableCellStyle> &result) {
  if (auto table_cell_properties = node.child("style:table-cell-properties")) {
    if (!result) {
      result = TableCellStyle();
    }

    if (auto vertical_align = read_vertical_align(
            table_cell_properties.attribute("style:vertical-align"))) {
      result->vertical_align = vertical_align;
    }
    if (auto background_color = read_color(
            table_cell_properties.attribute("fo:background-color"))) {
      result->background_color = background_color;
    }
    if (auto padding =
            read_measure(table_cell_properties.attribute("fo:padding"))) {
      result->padding = padding;
    }
    if (auto padding_right =
            read_measure(table_cell_properties.attribute("fo:padding-right"))) {
      result->padding.right = padding_right;
    }
    if (auto padding_top =
            read_measure(table_cell_properties.attribute("fo:padding-top"))) {
      result->padding.top = padding_top;
    }
    if (auto padding_left =
            read_measure(table_cell_properties.attribute("fo:padding-left"))) {
      result->padding.left = padding_left;
    }
    if (auto padding_bottom = read_measure(
            table_cell_properties.attribute("fo:padding-bottom"))) {
      result->padding.bottom = padding_bottom;
    }
    if (auto border =
            read_border(table_cell_properties.attribute("fo:border"))) {
      result->border = border;
    }
    if (auto border_right =
            read_border(table_cell_properties.attribute("fo:border-right"))) {
      result->border.right = border_right;
    }
    if (auto border_top =
            read_border(table_cell_properties.attribute("fo:border-top"))) {
      result->border.top = border_top;
    }
    if (auto border_left =
            read_border(table_cell_properties.attribute("fo:border-left"))) {
      result->border.left = border_left;
    }
    if (auto border_bottom =
            read_border(table_cell_properties.attribute("fo:border-bottom"))) {
      result->border.bottom = border_bottom;
    }
  }
}

void Style::resolve_graphic_style_(pugi::xml_node node,
                                   std::optional<GraphicStyle> &result) {
  if (auto graphic_properties = node.child("style:graphic-properties")) {
    if (!result) {
      result = GraphicStyle();
    }

    if (auto stroke_width =
            read_measure(graphic_properties.attribute("svg:stroke-width"))) {
      result->stroke_width = stroke_width;
    }
    if (auto stroke_color =
            read_color(graphic_properties.attribute("svg:stroke-color"))) {
      result->stroke_color = stroke_color;
    }
    if (auto fill_color =
            read_color(graphic_properties.attribute("draw:fill-color"))) {
      result->fill_color = fill_color;
    }
    if (auto vertical_align = read_vertical_align(
            graphic_properties.attribute("draw:textarea-vertical-align"))) {
      result->vertical_align = vertical_align;
    }
    if (auto text_wrap =
            read_text_wrap(graphic_properties.attribute("style:wrap"))) {
      result->text_wrap = text_wrap;
    }
  }
}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node content_root,
                             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

Style *StyleRegistry::style(const char *name) const {
  if (auto style_it = m_styles.find(name); style_it != std::end(m_styles)) {
    return style_it->second.get();
  }
  return {};
}

PageLayout StyleRegistry::page_layout(const std::string &name) const {
  if (auto page_layout_it = m_index_page_layout.find(name);
      page_layout_it != std::end(m_index_page_layout)) {
    return read_page_layout(page_layout_it->second);
  }
  return {};
}

pugi::xml_node StyleRegistry::master_page_node(const std::string &name) const {
  if (auto master_page_it = m_index_master_page.find(name);
      master_page_it != std::end(m_index_master_page)) {
    return master_page_it->second;
  }
  return {};
}

pugi::xml_node StyleRegistry::font_face_node(const std::string &name) const {
  if (auto font_face_it = m_index_font_face.find(name);
      font_face_it != std::end(m_index_font_face)) {
    return font_face_it->second;
  }
  return {};
}

std::optional<std::string> StyleRegistry::first_master_page() const {
  return m_first_master_page;
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

void StyleRegistry::generate_styles_() {
  for (auto &&e : m_index_default_style) {
    generate_default_style_(e.first, e.second);
  }

  for (auto &&e : m_index_style) {
    generate_style_(e.first, e.second);
  }
}

Style *StyleRegistry::generate_default_style_(const std::string &family,
                                              const pugi::xml_node node) {
  auto &&style = m_default_styles[family];
  if (style) {
    return style.get();
  }
  style = std::make_unique<Style>(this, family, node);
  return style.get();
}

Style *StyleRegistry::generate_style_(const std::string &name,
                                      const pugi::xml_node node) {
  auto &&style = m_styles[name];
  if (style) {
    return style.get();
  }

  Style *parent{nullptr};
  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    if (auto parent_node = m_index_style[parent_attr.value()]) {
      parent = generate_style_(parent_attr.value(), parent_node);
    }
  }

  Style *family{nullptr};
  if (auto family_attr = node.attribute("style:family"); family_attr) {
    family = generate_default_style_(family_attr.value(), {});
  }

  style = std::make_unique<Style>(this, name, node, parent, family);
  return style.get();
}

} // namespace odr::internal::odf
