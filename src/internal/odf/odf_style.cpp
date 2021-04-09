#include <cstring>
#include <functional>
#include <internal/odf/odf_style.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
std::any attribute_to_property(const std::string &name, const char *value) {
  return value;
}

std::any property_override_strategy(const std::string &name,
                                    const std::any &old_value,
                                    std::any new_value) {
  // TODO some properties use relative measures of their parent's properties
  // e.g. fo:font-size

  return new_value;
}

void resolve_properties(pugi::xml_node node,
                        std::unordered_map<std::string, std::any> &properties) {
  // TODO some property nodes have children e.g. <style:paragraph-properties>

  for (auto &&a : node.attributes()) {
    std::string name = a.name();
    properties[name] = property_override_strategy(
        name, properties[name], attribute_to_property(name, a.value()));
  }
}
} // namespace

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent) {
    result = m_parent->resolve();
  }

  resolve_properties(m_node.child("style:paragraph-properties"),
                     result.paragraph_properties);
  resolve_properties(m_node.child("style:text-properties"),
                     result.text_properties);

  resolve_properties(m_node.child("style:table-properties"),
                     result.table_properties);
  resolve_properties(m_node.child("style:table-column-properties"),
                     result.table_column_properties);
  resolve_properties(m_node.child("style:table-row-properties"),
                     result.table_row_properties);
  resolve_properties(m_node.child("style:table-cell-properties"),
                     result.table_cell_properties);

  resolve_properties(m_node.child("style:chart-properties"),
                     result.chart_properties);
  resolve_properties(m_node.child("style:drawing-page-properties"),
                     result.drawing_page_properties);
  resolve_properties(m_node.child("style:graphic-properties"),
                     result.graphic_properties);

  return result;
}

Styles::Styles(pugi::xml_node styles_root, pugi::xml_node content_root)
    : m_styles_root{styles_root}, m_content_root{content_root} {
  generate_indices();
  generate_styles();
}

std::shared_ptr<Style> Styles::style(const std::string &name) const {
  auto styleIt = m_styles.find(name);
  if (styleIt == std::end(m_styles)) {
    return {};
  }
  return styleIt->second;
}

void Styles::generate_indices() {
  if (auto font_face_decls = m_styles_root.child("office:font-face-decls");
      font_face_decls) {
    generate_indices(font_face_decls);
  }

  if (auto styles = m_styles_root.child("office:styles"); styles) {
    generate_indices(styles);
  }

  if (auto automatic_styles = m_styles_root.child("office:automatic-styles");
      automatic_styles) {
    generate_indices(automatic_styles);
  }

  if (auto master_styles = m_styles_root.child("office:master-styles");
      master_styles) {
    generate_indices(master_styles);
  }

  // content styles

  if (auto font_face_decls = m_content_root.child("office:font-face-decls");
      font_face_decls) {
    generate_indices(font_face_decls);
  }

  if (auto automatic_styles = m_content_root.child("office:automatic-styles");
      automatic_styles) {
    generate_indices(automatic_styles);
  }
}

void Styles::generate_indices(pugi::xml_node node) {
  for (auto &&e : node) {
    if (std::strcmp("style:font-face", e.name()) == 0) {
      m_index_font_face[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:default-style", e.name()) == 0) {
      m_index_default_style[e.attribute("style:family").value()] = e;
    } else if (std::strcmp("style:style", e.name()) == 0) {
      m_index_style[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:list-style", e.name()) == 0) {
      m_index_list_style[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:outline-style", e.name()) == 0) {
      m_index_outline_style[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:page-layout", e.name()) == 0) {
      m_index_page_layout[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:master-page", e.name()) == 0) {
      m_index_master_page[e.attribute("style:name").value()] = e;
    }
  }
}

void Styles::generate_styles() {
  for (auto &&e : m_index_default_style) {
    generate_default_style(e.first, e.second);
  }

  for (auto &&e : m_index_style) {
    generate_style(e.first, e.second);
  }
}

std::shared_ptr<Style> Styles::generate_default_style(const std::string &name,
                                                      pugi::xml_node node) {
  if (auto it = m_default_styles.find(name); it != std::end(m_default_styles)) {
    return it->second;
  }

  return m_default_styles[name] = std::make_shared<Style>(nullptr, node);
}

std::shared_ptr<Style> Styles::generate_style(const std::string &name,
                                              pugi::xml_node node) {
  if (auto styleIt = m_styles.find(name); styleIt != std::end(m_styles)) {
    return styleIt->second;
  }

  std::shared_ptr<Style> parent;

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    if (auto parent_style_it = m_index_style.find(parent_attr.value());
        parent_style_it != std::end(m_index_style)) {
      parent = generate_style(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  } else if (auto family_attr = node.attribute("style:family-name");
             family_attr) {
    if (auto family_style_it = m_index_default_style.find(name);
        family_style_it != std::end(m_index_default_style)) {
      parent =
          generate_default_style(parent_attr.value(), family_style_it->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Style>(parent, node);
}

} // namespace odr::internal::odf
