#include <cstring>
#include <internal/abstract/document_style.h>
#include <internal/common/property.h>
#include <internal/odf/odf_style.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
void attributes_to_map(pugi::xml_node node,
                       std::unordered_map<std::string, std::string> &map) {
  for (auto &&a : node.attributes()) {
    map[a.name()] = a.value();
  }
}

class PageStyle final : public abstract::PageStyle {
public:
  explicit PageStyle(pugi::xml_node page_layout_prop)
      : m_page_layout_prop{page_layout_prop} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> width() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:page-width"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> height() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:page-height"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_top() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:margin-top"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  margin_bottom() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:margin-bottom"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_left() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:margin-left"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_right() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("fo:margin-right"));
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  print_orientation() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_page_layout_prop.attribute("style:print-orientation"));
  }

private:
  pugi::xml_node m_page_layout_prop;
};

class TextStyle final : public abstract::TextStyle {
public:
  explicit TextStyle(
      std::unordered_map<std::string, std::string> text_properties)
      : m_text_properties{std::move(text_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> font_name() const final {
    return ResolvedStyle::lookup(m_text_properties, "style:font-name");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> font_size() const final {
    return ResolvedStyle::lookup(m_text_properties, "fo:font-size");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> font_weight() const final {
    return ResolvedStyle::lookup(m_text_properties, "fo:font-weight");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> font_style() const final {
    return ResolvedStyle::lookup(m_text_properties, "fo:font-style");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> font_color() const final {
    return ResolvedStyle::lookup(m_text_properties, "fo:color");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  background_color() const final {
    return ResolvedStyle::lookup(m_text_properties, "fo:background-color");
  }

private:
  std::unordered_map<std::string, std::string> m_text_properties;
};

class ParagraphStyle final : public abstract::ParagraphStyle {
public:
  explicit ParagraphStyle(
      std::unordered_map<std::string, std::string> paragraph_properties)
      : m_paragraph_properties{std::move(paragraph_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> text_align() const final {
    return ResolvedStyle::lookup(m_paragraph_properties, "fo:text-align");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_top() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraph_properties, "fo:margin-top");
    if (!result) {
      result = ResolvedStyle::lookup(m_paragraph_properties, "fo:margin");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  margin_bottom() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraph_properties, "fo:margin-bottom");
    if (!result) {
      result = ResolvedStyle::lookup(m_paragraph_properties, "fo:margin");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_left() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraph_properties, "fo:margin-left");
    if (!result) {
      result = ResolvedStyle::lookup(m_paragraph_properties, "fo:margin");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_right() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraph_properties, "fo:margin-right");
    if (!result) {
      result = ResolvedStyle::lookup(m_paragraph_properties, "fo:margin");
    }
    return result;
  }

private:
  std::unordered_map<std::string, std::string> m_paragraph_properties;
};

class TableStyle final : public abstract::TableStyle {
public:
  explicit TableStyle(
      std::unordered_map<std::string, std::string> table_properties)
      : m_table_properties{std::move(table_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> width() const final {
    return ResolvedStyle::lookup(m_table_properties, "style:width");
  }

private:
  std::unordered_map<std::string, std::string> m_table_properties;
};

class TableColumnStyle final : public abstract::TableColumnStyle {
public:
  explicit TableColumnStyle(
      std::unordered_map<std::string, std::string> table_column_properties)
      : m_table_column_properties{std::move(table_column_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> width() const final {
    return ResolvedStyle::lookup(m_table_column_properties,
                                 "style:column-width");
  }

private:
  std::unordered_map<std::string, std::string> m_table_column_properties;
};

class TableCellStyle final : public abstract::TableCellStyle {
public:
  explicit TableCellStyle(
      std::unordered_map<std::string, std::string> table_cell_properties)
      : m_table_cell_properties{std::move(table_cell_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> padding_top() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:padding-top");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:padding");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  padding_bottom() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:padding-bottom");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:padding");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> padding_left() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:padding-left");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:padding");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  padding_right() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:padding-right");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:padding");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> border_top() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:border-top");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:border");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  border_bottom() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:border-bottom");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:border");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> border_left() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:border-left");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:border");
    }
    return result;
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> border_right() const final {
    auto result =
        ResolvedStyle::lookup(m_table_cell_properties, "fo:border-right");
    if (!result) {
      result = ResolvedStyle::lookup(m_table_cell_properties, "fo:border");
    }
    return result;
  }

private:
  std::unordered_map<std::string, std::string> m_table_cell_properties;
};

class DrawingStyle final : public abstract::DrawingStyle {
public:
  explicit DrawingStyle(
      std::unordered_map<std::string, std::string> graphic_properties)
      : m_graphicProperties{std::move(graphic_properties)} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> stroke_width() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "svg:stroke-width");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> stroke_color() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "svg:stroke-color");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> fill_color() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "draw:fill-color");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  vertical_align() const final {
    return ResolvedStyle::lookup(m_graphicProperties,
                                 "draw:textarea-vertical-align");
  }

private:
  std::unordered_map<std::string, std::string> m_graphicProperties;
};
} // namespace

std::shared_ptr<abstract::Property>
ResolvedStyle::lookup(const std::unordered_map<std::string, std::string> &map,
                      const std::string &attribute) {
  auto it = map.find(attribute);
  std::optional<std::string> result;
  if (it != std::end(map)) {
    result = it->second;
  }
  return std::make_shared<abstract::ConstProperty>(result);
}

std::shared_ptr<abstract::TextStyle> ResolvedStyle::to_text_style() const {
  return std::make_shared<TextStyle>(text_properties);
}

std::shared_ptr<abstract::ParagraphStyle>
ResolvedStyle::to_paragraph_style() const {
  return std::make_shared<ParagraphStyle>(paragraph_properties);
}

std::shared_ptr<abstract::TableStyle> ResolvedStyle::to_table_style() const {
  return std::make_shared<TableStyle>(table_properties);
}

std::shared_ptr<abstract::TableColumnStyle>
ResolvedStyle::to_table_column_style() const {
  return std::make_shared<TableColumnStyle>(paragraph_properties);
}

std::shared_ptr<abstract::TableCellStyle>
ResolvedStyle::to_table_cell_style() const {
  return std::make_shared<TableCellStyle>(table_cell_properties);
}

std::shared_ptr<abstract::DrawingStyle>
ResolvedStyle::to_drawing_style() const {
  return std::make_shared<DrawingStyle>(graphic_properties);
}

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent) {
    result = m_parent->resolve();
  }

  // TODO some property nodes have children e.g. <style:paragraph-properties>
  // TODO some properties use relative measures of their parent's properties
  // e.g. fo:font-size

  attributes_to_map(m_node.child("style:paragraph-properties"),
                    result.paragraph_properties);
  attributes_to_map(m_node.child("style:text-properties"),
                    result.text_properties);

  attributes_to_map(m_node.child("style:table-properties"),
                    result.table_properties);
  attributes_to_map(m_node.child("style:table-column-properties"),
                    result.table_column_properties);
  attributes_to_map(m_node.child("style:table-row-properties"),
                    result.table_row_properties);
  attributes_to_map(m_node.child("style:table-cell-properties"),
                    result.table_cell_properties);

  attributes_to_map(m_node.child("style:chart-properties"),
                    result.chart_properties);
  attributes_to_map(m_node.child("style:drawing-page-properties"),
                    result.drawing_page_properties);
  attributes_to_map(m_node.child("style:graphic-properties"),
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

std::shared_ptr<abstract::PageStyle>
Styles::page_style(const std::string &name) const {
  auto page_layout_it = m_index_page_layout.find(name);
  if (page_layout_it == std::end(m_index_page_layout)) {
    throw 1; // TODO exception or optional
  }
  auto page_layout_prop =
      page_layout_it->second.child("style:page-layout-properties");

  return std::make_shared<PageStyle>(page_layout_prop);
}

std::shared_ptr<abstract::PageStyle>
Styles::master_page_style(const std::string &name) const {
  auto master_page_it = m_index_master_page.find(name);
  if (master_page_it == std::end(m_index_master_page)) {
    throw 1; // TODO exception or optional
  }
  const std::string page_layout_name =
      master_page_it->second.attribute("style:page-layout-name").value();
  return page_style(page_layout_name);
}

std::shared_ptr<abstract::PageStyle> Styles::default_page_style() const {
  const pugi::xml_node master_styles =
      m_styles_root.child("office:master-styles");
  const pugi::xml_node master_style = master_styles.first_child();
  const std::string page_layout_name =
      master_style.attribute("style:page-layout-name").value();
  return page_style(page_layout_name);
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
