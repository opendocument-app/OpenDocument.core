#include <cstring>
#include <functional>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {

const char *read_optional_string(pugi::xml_attribute attribute) {
  if (attribute) {
    return attribute.value();
  }
  return {};
}

std::optional<TextAlign>
read_optional_text_align(pugi::xml_attribute attribute) {
  if (!attribute) {
    return {};
  }
  auto value = attribute.value();
  if (std::strcmp("left", value) == 0) {
    return TextAlign::left;
  }
  if (std::strcmp("right", value) == 0) {
    return TextAlign::right;
  }
  if (std::strcmp("center", value) == 0) {
    return TextAlign::center;
  }
  return {};
}

std::optional<VerticalAlign>
read_optional_vertical_align(pugi::xml_attribute attribute) {
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

PageLayout read_page_layout(pugi::xml_node node) {
  PageLayout result;

  auto page_layout_properties = node.child("style:page-layout-properties");

  result.width =
      read_optional_string(page_layout_properties.attribute("fo:page-width"));
  result.height =
      read_optional_string(page_layout_properties.attribute("fo:page-height"));
  result.print_orientation = read_optional_string(
      page_layout_properties.attribute("style:print-orientation"));
  result.margin =
      read_optional_string(page_layout_properties.attribute("fo:margin"));
  result.margin.right =
      read_optional_string(page_layout_properties.attribute("fo:margin-right"));
  result.margin.top =
      read_optional_string(page_layout_properties.attribute("fo:margin-top"));
  result.margin.left =
      read_optional_string(page_layout_properties.attribute("fo:margin-left"));
  result.margin.bottom = read_optional_string(
      page_layout_properties.attribute("fo:margin-bottom"));

  return result;
}

} // namespace

Style::Style() = default;

Style::Style(std::string family, pugi::xml_node node)
    : m_name{std::move(family)}, m_node{node} {
  resolve_style_();
}

Style::Style(std::string name, pugi::xml_node node, Style *parent,
             Style *family)
    : m_name{std::move(name)}, m_node{node}, m_parent{parent}, m_family{
                                                                   family} {
  if (Style *copy_from = m_parent ? m_parent : m_family) {
    m_resolved = copy_from->m_resolved;
  }

  resolve_style_();
}

std::string Style::name() const { return m_name; }

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
  if (auto text_properties = node.child("style:text-properties")) {
    if (!result) {
      result = TextStyle();
    }

    result->font_name =
        read_optional_string(text_properties.attribute("style:font-name"));
    result->font_size =
        read_optional_string(text_properties.attribute("fo:font-size"));
    result->font_weight =
        read_optional_string(text_properties.attribute("fo:font-weight"));
    result->font_style =
        read_optional_string(text_properties.attribute("fo:font-style"));
    result->font_underline = read_optional_string(
        text_properties.attribute("style:text-underline-style"));
    result->font_line_through = read_optional_string(
        text_properties.attribute("style:text-line-through-style"));
    result->font_shadow =
        read_optional_string(text_properties.attribute("fo:text-shadow"));
    result->font_color =
        read_optional_string(text_properties.attribute("fo:color"));
    result->background_color =
        read_optional_string(text_properties.attribute("fo:background-color"));
  }
}

void Style::resolve_paragraph_style_(pugi::xml_node node,
                                     std::optional<ParagraphStyle> &result) {
  if (auto paragraph_properties = node.child("style:paragraph-properties")) {
    if (!result) {
      result = ParagraphStyle();
    }

    result->text_align = read_optional_text_align(
        paragraph_properties.attribute("fo:text-align"));
    result->margin =
        read_optional_string(paragraph_properties.attribute("fo:margin"));
    result->margin.right =
        read_optional_string(paragraph_properties.attribute("fo:margin-right"));
    result->margin.top =
        read_optional_string(paragraph_properties.attribute("fo:margin-top"));
    result->margin.left =
        read_optional_string(paragraph_properties.attribute("fo:margin-left"));
    result->margin.bottom = read_optional_string(
        paragraph_properties.attribute("fo:margin-bottom"));
  }
}

void Style::resolve_table_style_(pugi::xml_node node,
                                 std::optional<TableStyle> &result) {
  if (auto table_properties = node.child("style:table-properties")) {
    if (!result) {
      result = TableStyle();
    }

    result->width =
        read_optional_string(table_properties.attribute("style:width"));
  }
}

void Style::resolve_table_column_style_(
    pugi::xml_node node, std::optional<TableColumnStyle> &result) {
  if (auto table_column_properties =
          node.child("style:table-column-properties")) {
    if (!result) {
      result = TableColumnStyle();
    }

    result->width = read_optional_string(
        table_column_properties.attribute("style:column-width"));
  }
}

void Style::resolve_table_row_style_(pugi::xml_node node,
                                     std::optional<TableRowStyle> &result) {
  if (auto table_row_properties = node.child("style:table-row-properties")) {
    if (!result) {
      result = TableRowStyle();
    }

    result->height = read_optional_string(
        table_row_properties.attribute("style:row-height"));
  }
}

void Style::resolve_table_cell_style_(pugi::xml_node node,
                                      std::optional<TableCellStyle> &result) {
  if (auto table_cell_properties = node.child("style:table-cell-properties")) {
    if (!result) {
      result = TableCellStyle();
    }

    result->vertical_align = read_optional_vertical_align(
        table_cell_properties.attribute("style:vertical-align"));
    result->background_color = read_optional_string(
        table_cell_properties.attribute("fo:background-color"));
    result->padding =
        read_optional_string(table_cell_properties.attribute("fo:padding"));
    result->padding.right = read_optional_string(
        table_cell_properties.attribute("fo:padding-right"));
    result->padding.top =
        read_optional_string(table_cell_properties.attribute("fo:padding-top"));
    result->padding.left = read_optional_string(
        table_cell_properties.attribute("fo:padding-left"));
    result->padding.bottom = read_optional_string(
        table_cell_properties.attribute("fo:padding-bottom"));
    result->border =
        read_optional_string(table_cell_properties.attribute("fo:border"));
    result->border.right = read_optional_string(
        table_cell_properties.attribute("fo:border-right"));
    result->border.top =
        read_optional_string(table_cell_properties.attribute("fo:border-top"));
    result->border.left =
        read_optional_string(table_cell_properties.attribute("fo:border-left"));
    result->border.bottom = read_optional_string(
        table_cell_properties.attribute("fo:border-bottom"));
  }
}

void Style::resolve_graphic_style_(pugi::xml_node node,
                                   std::optional<GraphicStyle> &result) {
  if (auto graphic_properties = node.child("style:graphic-properties")) {
    if (!result) {
      result = GraphicStyle();
    }

    result->stroke_width =
        read_optional_string(graphic_properties.attribute("svg:stroke-width"));
    result->stroke_color =
        read_optional_string(graphic_properties.attribute("svg:stroke-color"));
    result->fill_color =
        read_optional_string(graphic_properties.attribute("draw:fill-color"));
    result->vertical_align = read_optional_vertical_align(
        graphic_properties.attribute("draw:textarea-vertical-align"));
  }
}

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node content_root,
                             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

Style *StyleRegistry::style(const std::string &name) const {
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

pugi::xml_node
StyleRegistry::master_page_node(const std::string &master_page_name) const {
  if (auto master_page_it = m_index_master_page.find(master_page_name);
      master_page_it != std::end(m_index_master_page)) {
    return master_page_it->second;
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
  style = std::make_unique<Style>(family, node);
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
    parent = generate_style_(parent_attr.value(),
                             m_index_style.at(parent_attr.value()));
  }

  Style *family{nullptr};
  if (auto family_attr = node.attribute("style:family"); family_attr) {
    family = generate_default_style_(family_attr.value(), {});
  }

  style = std::make_unique<Style>(name, node, parent, family);
  return style.get();
}

} // namespace odr::internal::odf
