#include <functional>
#include <internal/abstract/document.h>
#include <internal/odf/odf_style.h>
#include <internal/util/string_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {

class Style : public virtual abstract::Style {
public:
  StyleRegistry::StyleCollection *m_collection;

  explicit Style(StyleRegistry::StyleCollection *collection)
      : m_collection{collection} {}

  std::optional<std::string> name() const final;
  pugi::xml_node node() const;
};

class StringAttributeProperty : public abstract::Property {
public:
  StringAttributeProperty(const char *property_class_name,
                          const char *attribute_name)
      : m_property_class_name{property_class_name}, m_attribute_name{
                                                        attribute_name} {}

  [[nodiscard]] std::optional<std::string>
  value(const abstract::Document *document, const abstract::Element *element,
        const abstract::Style *abstract_style,
        const StyleContext style_context) const final {
    auto style = dynamic_cast<const Style *>(abstract_style);
    if (auto attribute = style->node()
                             .child(m_property_class_name)
                             .attribute(m_attribute_name)) {
      return attribute.value();
    }
    if (auto parent = style->parent()) {
      return value(document, element, parent, style_context);
    }
    return {};
  }

private:
  const char *m_property_class_name;
  const char *m_attribute_name;
};

class DirectionalProperty : public abstract::DirectionalProperty {
public:
  class Property : public abstract::Property {
  public:
    Property(const char *property_class_name, const char *attribute_name,
             const char *default_attribute_name)
        : m_attribute{property_class_name, attribute_name},
          m_default_attribute{property_class_name, default_attribute_name} {}

    [[nodiscard]] std::optional<std::string>
    value(const abstract::Document *document, const abstract::Element *element,
          const abstract::Style *style,
          const StyleContext style_context) const final {
      if (auto value =
              m_attribute.value(document, element, style, style_context)) {
        return value;
      }
      return m_default_attribute.value(document, element, style, style_context);
    }

  private:
    StringAttributeProperty m_attribute;
    StringAttributeProperty m_default_attribute;
  };

  DirectionalProperty(const char *property_class_name,
                      const char *default_attribute_name,
                      const char *right_attribute_name,
                      const char *top_attribute_name,
                      const char *left_attribute_name,
                      const char *bottom_attribute_name)
      : m_right{property_class_name, right_attribute_name,
                default_attribute_name},
        m_top{property_class_name, top_attribute_name, default_attribute_name},
        m_left{property_class_name, left_attribute_name,
               default_attribute_name},
        m_bottom{property_class_name, bottom_attribute_name,
                 default_attribute_name} {}

  [[nodiscard]] abstract::Property *right(const abstract::Document *) final {
    return &m_right;
  }

  [[nodiscard]] abstract::Property *top(const abstract::Document *) final {
    return &m_top;
  }

  [[nodiscard]] abstract::Property *left(const abstract::Document *) final {
    return &m_left;
  }

  [[nodiscard]] abstract::Property *bottom(const abstract::Document *) final {
    return &m_bottom;
  }

private:
  Property m_right;
  Property m_top;
  Property m_left;
  Property m_bottom;
};

class TextStyle final : public virtual Style,
                        public virtual abstract::TextStyle {
public:
  using Style::Style;

  const abstract::TextStyle *parent() const final;

  [[nodiscard]] abstract::Property *
  font_name(const abstract::Document *) final {
    static StringAttributeProperty font_name{"style:text-properties",
                                             "style:font-name"};
    return &font_name;
  }

  [[nodiscard]] abstract::Property *
  font_size(const abstract::Document *) final {
    static StringAttributeProperty font_size{"style:text-properties",
                                             "fo:font-size"};
    return &font_size;
  }

  [[nodiscard]] abstract::Property *
  font_weight(const abstract::Document *) final {
    static StringAttributeProperty font_weight{"style:text-properties",
                                               "fo:font-weight"};
    return &font_weight;
  }

  [[nodiscard]] abstract::Property *
  font_style(const abstract::Document *) final {
    static StringAttributeProperty font_style{"style:text-properties",
                                              "fo:font-style"};
    return &font_style;
  }

  [[nodiscard]] abstract::Property *
  font_underline(const abstract::Document *) final {
    static StringAttributeProperty font_underline{"style:text-properties",
                                                  "style:text-underline-style"};
    return &font_underline;
  }

  [[nodiscard]] abstract::Property *
  font_line_through(const abstract::Document *) final {
    static StringAttributeProperty font_line_through{
        "style:text-properties", "style:text-line-through-style"};
    return &font_line_through;
  }

  [[nodiscard]] abstract::Property *
  font_shadow(const abstract::Document *) final {
    static StringAttributeProperty font_shadow{"style:text-properties",
                                               "fo:text-shadow"};
    return &font_shadow;
  }

  [[nodiscard]] abstract::Property *
  font_color(const abstract::Document *) final {
    static StringAttributeProperty font_color{"style:text-properties",
                                              "fo:color"};
    return &font_color;
  }

  [[nodiscard]] abstract::Property *
  background_color(const abstract::Document *) final {
    static StringAttributeProperty background_color{"style:text-properties",
                                                    "fo:background-color"};
    return &background_color;
  }
};

class ParagraphStyle final : public virtual Style,
                             public virtual abstract::ParagraphStyle {
public:
  using Style::Style;

  const abstract::ParagraphStyle *parent() const final;

  [[nodiscard]] abstract::Property *
  text_align(const abstract::Document *) final {
    static StringAttributeProperty text_align{"style:paragraph-properties",
                                              "fo:text-align"};
    return &text_align;
  }

  [[nodiscard]] abstract::DirectionalProperty *
  margin(const abstract::Document *) final {
    static DirectionalProperty margin{"style:paragraph-properties",
                                      "fo:margin",
                                      "fo:margin-right",
                                      "fo:margin-top",
                                      "fo:margin-left",
                                      "fo:margin-bottom"};
    return &margin;
  }
};

class TableStyle final : public virtual Style,
                         public virtual abstract::TableStyle {
public:
  using Style::Style;

  const abstract::TableStyle *parent() const final;

  [[nodiscard]] abstract::Property *width(const abstract::Document *) final {
    static StringAttributeProperty width{"style:table-properties",
                                         "style:width"};
    return &width;
  }
};

class TableColumnStyle final : public virtual Style,
                               public virtual abstract::TableColumnStyle {
public:
  using Style::Style;

  const abstract::TableColumnStyle *parent() const final;

  [[nodiscard]] abstract::Property *width(const abstract::Document *) final {
    static StringAttributeProperty width{"style:table-column-properties",
                                         "style:column-width"};
    return &width;
  }
};

class TableRowStyle final : public virtual Style,
                            public virtual abstract::TableRowStyle {
public:
  using Style::Style;

  const abstract::TableRowStyle *parent() const final;

  [[nodiscard]] abstract::Property *height(const abstract::Document *) final {
    static StringAttributeProperty height{"style:table-row-properties",
                                          "style:row-height"};
    return &height;
  }
};

class TableCellStyle final : public virtual Style,
                             public virtual abstract::TableCellStyle {
public:
  using Style::Style;

  const abstract::TableCellStyle *parent() const final;

  [[nodiscard]] abstract::Property *
  vertical_align(const abstract::Document *) final {
    static StringAttributeProperty vertical_align{"style:table-cell-properties",
                                                  "style:vertical-align"};
    return &vertical_align;
  }

  [[nodiscard]] abstract::Property *
  background_color(const abstract::Document *) final {
    static StringAttributeProperty background_color{
        "style:table-cell-properties", "fo:background-color"};
    return &background_color;
  }

  [[nodiscard]] abstract::DirectionalProperty *
  padding(const abstract::Document *) final {
    static DirectionalProperty padding{"style:table-cell-properties",
                                       "fo:padding",
                                       "fo:padding-right",
                                       "fo:padding-top",
                                       "fo:padding-left",
                                       "fo:padding-bottom"};
    return &padding;
  }

  [[nodiscard]] abstract::DirectionalProperty *
  border(const abstract::Document *) final {
    static DirectionalProperty border{"style:table-cell-properties",
                                      "fo:border",
                                      "fo:border-right",
                                      "fo:border-top",
                                      "fo:border-left",
                                      "fo:border-bottom"};
    return &border;
  }
};

class GraphicStyle final : public virtual Style,
                           public virtual abstract::GraphicStyle {
public:
  using Style::Style;

  const abstract::GraphicStyle *parent() const final;

  [[nodiscard]] abstract::Property *
  stroke_width(const abstract::Document *) final {
    static StringAttributeProperty stroke_width{"style:graphic-properties",
                                                "svg:stroke-width"};
    return &stroke_width;
  }

  [[nodiscard]] abstract::Property *
  stroke_color(const abstract::Document *) final {
    static StringAttributeProperty stroke_color{"style:graphic-properties",
                                                "svg:stroke-color"};
    return &stroke_color;
  }

  [[nodiscard]] abstract::Property *
  fill_color(const abstract::Document *) final {
    static StringAttributeProperty fill_color{"style:graphic-properties",
                                              "draw:fill-color"};
    return &fill_color;
  }

  [[nodiscard]] abstract::Property *
  vertical_align(const abstract::Document *) final {
    static StringAttributeProperty vertical_align{
        "style:graphic-properties", "draw:textarea-vertical-align"};
    return &vertical_align;
  }
};

class PageLayout final : public virtual abstract::PageLayout {
public:
  class StringAttributeProperty : public abstract::Property {
  public:
    StringAttributeProperty(const char *property_class_name,
                            const char *attribute_name)
        : m_property_class_name{property_class_name}, m_attribute_name{
                                                          attribute_name} {}

    [[nodiscard]] std::optional<std::string>
    value(const abstract::Document *, const abstract::Element *,
          const abstract::Style *style, const StyleContext) const final {
      auto page_layout = dynamic_cast<const PageLayout *>(style);
      if (auto attribute = page_layout->m_node.child(m_property_class_name)
                               .attribute(m_attribute_name)) {
        return attribute.value();
      }
      return {};
    }

  private:
    const char *m_property_class_name;
    const char *m_attribute_name;
  };

  class DirectionalProperty : public abstract::DirectionalProperty {
  public:
    class Property : public abstract::Property {
    public:
      Property(const char *property_class_name, const char *attribute_name,
               const char *default_attribute_name)
          : m_attribute{property_class_name, attribute_name},
            m_default_attribute{property_class_name, default_attribute_name} {}

      [[nodiscard]] std::optional<std::string>
      value(const abstract::Document *document,
            const abstract::Element *element, const abstract::Style *style,
            const StyleContext style_context) const final {
        if (auto value =
                m_attribute.value(document, element, style, style_context)) {
          return value;
        }
        return m_default_attribute.value(document, element, style,
                                         style_context);
      }

    private:
      StringAttributeProperty m_attribute;
      StringAttributeProperty m_default_attribute;
    };

    DirectionalProperty(const char *property_class_name,
                        const char *default_attribute_name,
                        const char *right_attribute_name,
                        const char *top_attribute_name,
                        const char *left_attribute_name,
                        const char *bottom_attribute_name)
        : m_right{property_class_name, right_attribute_name,
                  default_attribute_name},
          m_top{property_class_name, top_attribute_name,
                default_attribute_name},
          m_left{property_class_name, left_attribute_name,
                 default_attribute_name},
          m_bottom{property_class_name, bottom_attribute_name,
                   default_attribute_name} {}

    [[nodiscard]] abstract::Property *right(const abstract::Document *) final {
      return &m_right;
    }

    [[nodiscard]] abstract::Property *top(const abstract::Document *) final {
      return &m_top;
    }

    [[nodiscard]] abstract::Property *left(const abstract::Document *) final {
      return &m_left;
    }

    [[nodiscard]] abstract::Property *bottom(const abstract::Document *) final {
      return &m_bottom;
    }

  private:
    Property m_right;
    Property m_top;
    Property m_left;
    Property m_bottom;
  };

  PageLayout(std::string name, pugi::xml_node node)
      : m_name{std::move(name)}, m_node{node} {}

  std::optional<std::string> name() const final { return m_name; }

  const abstract::PageLayout *parent() const final { return nullptr; }

  [[nodiscard]] abstract::Property *width(const abstract::Document *) final {
    static StringAttributeProperty width{"style:page-layout-properties",
                                         "fo:page-width"};
    return &width;
  }

  [[nodiscard]] abstract::Property *height(const abstract::Document *) final {
    static StringAttributeProperty height{"style:page-layout-properties",
                                          "fo:page-height"};
    return &height;
  }

  [[nodiscard]] abstract::Property *
  print_orientation(const abstract::Document *) final {
    static StringAttributeProperty print_orientation{
        "style:page-layout-properties", "style:print-orientation"};
    return &print_orientation;
  }

  [[nodiscard]] abstract::DirectionalProperty *
  margin(const abstract::Document *) final {
    static DirectionalProperty margin{"style:page-layout-properties",
                                      "fo:margin",
                                      "fo:margin-right",
                                      "fo:margin-top",
                                      "fo:margin-left",
                                      "fo:margin-bottom"};
    return &margin;
  }

private:
  std::string m_name;
  pugi::xml_node m_node;
};

} // namespace

class StyleRegistry::StyleCollection final {
public:
  std::string name;
  std::string family;
  pugi::xml_node node;
  StyleCollection *parent;

  TextStyle text_style;
  ParagraphStyle paragraph_style;
  TableStyle table_style;
  TableColumnStyle table_column_style;
  TableRowStyle table_row_style;
  TableCellStyle table_cell_style;
  GraphicStyle graphic_style;

  StyleCollection(std::string name, std::string family, pugi::xml_node node,
                  StyleCollection *parent)
      : name{std::move(name)}, family{std::move(family)}, node{node},
        parent{parent}, text_style{this}, paragraph_style{this},
        table_style{this}, table_column_style{this}, table_row_style{this},
        table_cell_style{this}, graphic_style{this} {}
};

namespace {

std::optional<std::string> Style::name() const { return m_collection->name; }

pugi::xml_node Style::node() const { return m_collection->node; }

const abstract::TextStyle *TextStyle::parent() const {
  return &m_collection->parent->text_style;
}

const abstract::ParagraphStyle *ParagraphStyle::parent() const {
  return &m_collection->parent->paragraph_style;
}

const abstract::TableStyle *TableStyle::parent() const {
  return &m_collection->parent->table_style;
}

const abstract::TableColumnStyle *TableColumnStyle::parent() const {
  return &m_collection->parent->table_column_style;
}

const abstract::TableRowStyle *TableRowStyle::parent() const {
  return &m_collection->parent->table_row_style;
}

const abstract::TableCellStyle *TableCellStyle::parent() const {
  return &m_collection->parent->table_cell_style;
}

const abstract::GraphicStyle *GraphicStyle::parent() const {
  return &m_collection->parent->graphic_style;
}

} // namespace

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node content_root,
                             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

abstract::TextStyle *StyleRegistry::text_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->text_style : nullptr;
}

abstract::ParagraphStyle *
StyleRegistry::paragraph_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->paragraph_style : nullptr;
}

abstract::TableStyle *
StyleRegistry::table_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->table_style : nullptr;
}

abstract::TableColumnStyle *
StyleRegistry::table_column_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->table_column_style : nullptr;
}

abstract::TableRowStyle *
StyleRegistry::table_row_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->table_row_style : nullptr;
}

abstract::TableCellStyle *
StyleRegistry::table_cell_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->table_cell_style : nullptr;
}

abstract::GraphicStyle *
StyleRegistry::graphic_style(const std::string &name) const {
  auto style = style_(name);
  return style ? &style->graphic_style : nullptr;
}

abstract::PageLayout *
StyleRegistry::page_layout(const std::string &name) const {
  if (auto styles_it = m_page_layouts.find(name);
      styles_it != std::end(m_page_layouts)) {
    return styles_it->second.get();
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

StyleRegistry::StyleCollection *
StyleRegistry::generate_default_style_(const std::string &name,
                                       const pugi::xml_node node) {
  // TODO unique name
  auto &&style = m_default_styles[name];
  if (!style) {
    style = std::make_unique<StyleCollection>(name, name, node, nullptr);
  }
  return style.get();
}

StyleRegistry::StyleCollection *
StyleRegistry::generate_style_(const std::string &name,
                               const pugi::xml_node node) {
  // TODO unique name
  auto &&style = m_styles[name];
  if (style) {
    return style.get();
  }

  StyleCollection *parent{nullptr};

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    parent = generate_style_(parent_attr.value(),
                             m_index_style.at(parent_attr.value()));
  } else if (auto family_attr = node.attribute("style:family"); family_attr) {
    // TODO should not use family as parent
    parent = generate_default_style_(
        parent_attr.value(), m_index_default_style.at(parent_attr.value()));
  }

  style = std::make_unique<StyleCollection>(
      name, node.attribute("style:family").value(), node, parent);
  return style.get();
}

void StyleRegistry::generate_page_layout_(const std::string &name,
                                          const pugi::xml_node node) {
  // TODO unique name
  auto &&style = m_page_layouts[name];
  if (!style) {
    style = std::make_unique<PageLayout>(name, node);
  }
}

StyleRegistry::StyleCollection *
StyleRegistry::style_(const std::string &name) const {
  if (auto styles_it = m_styles.find(name); styles_it != std::end(m_styles)) {
    return styles_it->second.get();
  }
  return {};
}

} // namespace odr::internal::odf
