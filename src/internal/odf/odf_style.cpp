#include <functional>
#include <internal/abstract/document.h>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {

std::optional<std::string> read_optional_string(pugi::xml_attribute attribute) {
  if (attribute) {
    return attribute.value();
  }
  return {};
}

class Style final : public abstract::Style {
public:
  Style(std::string name, std::string family, pugi::xml_node node,
        abstract::Style *parent)
      : m_name{std::move(name)}, m_family{std::move(family)}, m_node{node},
        m_parent{parent} {}

  std::optional<std::string> name(const abstract::Document *,
                                  const abstract::Element *) const final {
    return m_name;
  }

  bool text_style(const abstract::Document *document,
                  const abstract::Element *element,
                  const StyleDepth style_depth, TextStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed = m_parent->text_style(document, element, style_depth, result);
    }

    if (auto text_properties = m_node.child("style:text-properties")) {
      result.font_name =
          read_optional_string(text_properties.attribute("style:font-name"));
      result.font_size =
          read_optional_string(text_properties.attribute("fo:font-size"));
      result.font_weight =
          read_optional_string(text_properties.attribute("fo:font-weight"));
      result.font_style =
          read_optional_string(text_properties.attribute("fo:font-style"));
      result.font_underline = read_optional_string(
          text_properties.attribute("style:text-underline-style"));
      result.font_line_through = read_optional_string(
          text_properties.attribute("style:text-line-through-style"));
      result.font_shadow =
          read_optional_string(text_properties.attribute("fo:text-shadow"));
      result.font_color =
          read_optional_string(text_properties.attribute("fo:color"));
      result.font_shadow = read_optional_string(
          text_properties.attribute("fo:background-color"));

      changed = true;
    }

    return changed;
  }

  bool paragraph_style(const abstract::Document *document,
                       const abstract::Element *element, StyleDepth style_depth,
                       ParagraphStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed =
          m_parent->paragraph_style(document, element, style_depth, result);
    }

    if (auto paragraph_properties =
            m_node.child("style:paragraph-properties")) {
      result.text_align =
          read_optional_string(paragraph_properties.attribute("fo:text-align"));
      result.margin =
          read_optional_string(paragraph_properties.attribute("fo:margin"));
      result.margin.right = read_optional_string(
          paragraph_properties.attribute("fo:margin-right"));
      result.margin.top =
          read_optional_string(paragraph_properties.attribute("fo:margin-top"));
      result.margin.left = read_optional_string(
          paragraph_properties.attribute("fo:margin-left"));
      result.margin.bottom = read_optional_string(
          paragraph_properties.attribute("fo:margin-bottom"));

      changed = true;
    }

    return changed;
  }

  bool table_style(const abstract::Document *document,
                   const abstract::Element *element, StyleDepth style_depth,
                   TableStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed = m_parent->table_style(document, element, style_depth, result);
    }

    if (auto table_properties = m_node.child("style:table-properties")) {
      result.width =
          read_optional_string(table_properties.attribute("style:width"));

      changed = true;
    }

    return changed;
  }

  bool table_column_style(const abstract::Document *document,
                          const abstract::Element *element,
                          StyleDepth style_depth,
                          TableColumnStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed =
          m_parent->table_column_style(document, element, style_depth, result);
    }

    if (auto table_column_properties =
            m_node.child("style:table-column-properties")) {
      result.width = read_optional_string(
          table_column_properties.attribute("style:column-width"));

      changed = true;
    }

    return changed;
  }

  bool table_row_style(const abstract::Document *document,
                       const abstract::Element *element, StyleDepth style_depth,
                       TableRowStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed =
          m_parent->table_row_style(document, element, style_depth, result);
    }

    if (auto table_row_properties =
            m_node.child("style:table-row-properties")) {
      result.height = read_optional_string(
          table_row_properties.attribute("style:row-height"));

      changed = true;
    }

    return changed;
  }

  bool table_cell_style(const abstract::Document *document,
                        const abstract::Element *element,
                        StyleDepth style_depth,
                        TableCellStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed =
          m_parent->table_cell_style(document, element, style_depth, result);
    }

    if (auto table_cell_properties =
            m_node.child("style:table-cell-properties")) {
      result.vertical_align = read_optional_string(
          table_cell_properties.attribute("style:vertical-align"));
      result.background_color = read_optional_string(
          table_cell_properties.attribute("fo:background-color"));
      result.padding =
          read_optional_string(table_cell_properties.attribute("fo:padding"));
      result.padding.right = read_optional_string(
          table_cell_properties.attribute("fo:padding-right"));
      result.padding.top = read_optional_string(
          table_cell_properties.attribute("fo:padding-top"));
      result.padding.left = read_optional_string(
          table_cell_properties.attribute("fo:padding-left"));
      result.padding.bottom = read_optional_string(
          table_cell_properties.attribute("fo:padding-bottom"));
      result.border =
          read_optional_string(table_cell_properties.attribute("fo:border"));
      result.border.right = read_optional_string(
          table_cell_properties.attribute("fo:border-right"));
      result.border.top = read_optional_string(
          table_cell_properties.attribute("fo:border-top"));
      result.border.left = read_optional_string(
          table_cell_properties.attribute("fo:border-left"));
      result.border.bottom = read_optional_string(
          table_cell_properties.attribute("fo:border-bottom"));

      changed = true;
    }

    return changed;
  }

  bool graphic_style(const abstract::Document *document,
                     const abstract::Element *element, StyleDepth style_depth,
                     GraphicStyle &result) const final {
    bool changed = false;

    if (m_parent && (style_depth != StyleDepth::single_style)) {
      changed = m_parent->graphic_style(document, element, style_depth, result);
    }

    if (auto graphic_properties = m_node.child("style:graphic-properties")) {
      result.stroke_width = read_optional_string(
          graphic_properties.attribute("svg:stroke-width"));
      result.stroke_color = read_optional_string(
          graphic_properties.attribute("svg:stroke-color"));
      result.fill_color =
          read_optional_string(graphic_properties.attribute("draw:fill-color"));
      result.vertical_align = read_optional_string(
          graphic_properties.attribute("draw:textarea-vertical-align"));

      changed = true;
    }

    return changed;
  }

private:
  std::string m_name;
  std::string m_family;
  pugi::xml_node m_node;
  abstract::Style *m_parent;
};

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

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node content_root,
                             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

abstract::Style *StyleRegistry::style(const std::string &name) const {
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

  for (auto &&e : m_index_page_layout) {
    generate_page_layout_(e.first, e.second);
  }
}

abstract::Style *
StyleRegistry::generate_default_style_(const std::string &name,
                                       const pugi::xml_node node) {
  // TODO unique name
  auto &&style = m_default_styles[name];
  if (!style) {
    style = std::make_unique<Style>(name, name, node, nullptr);
  }
  return style.get();
}

abstract::Style *StyleRegistry::generate_style_(const std::string &name,
                                                const pugi::xml_node node) {
  // TODO unique name
  auto &&style = m_styles[name];
  if (style) {
    return style.get();
  }

  abstract::Style *parent{nullptr};

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    parent = generate_style_(parent_attr.value(),
                             m_index_style.at(parent_attr.value()));
  } else if (auto family_attr = node.attribute("style:family"); family_attr) {
    // TODO should not use family as parent
    parent = generate_default_style_(
        parent_attr.value(), m_index_default_style.at(parent_attr.value()));
  }

  style = std::make_unique<Style>(name, node.attribute("style:family").value(),
                                  node, parent);
  return style.get();
}

} // namespace odr::internal::odf
