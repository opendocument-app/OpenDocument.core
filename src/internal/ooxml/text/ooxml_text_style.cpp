#include <internal/abstract/document.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <internal/util/property_util.h>

namespace odr::internal::ooxml::text {

namespace {

class Style final : public abstract::Style {
public:
  Style(const Style *parent, pugi::xml_node node)
      : m_parent{parent}, m_node{node} {}

  class StringAttributeProperty : public abstract::Property {
  public:
    StringAttributeProperty(const char *property_class_name,
                            const char *attribute_name)
        : m_property_class_name{property_class_name}, m_attribute_name{
                                                          attribute_name} {}

    [[nodiscard]] std::optional<std::string>
    value(const abstract::Document *document, const abstract::Element *element,
          const abstract::Style *style) const final {
      if (auto attribute = dynamic_cast<const Style *>(style)
                               ->m_node.child(m_property_class_name)
                               .attribute(m_attribute_name)) {
        return attribute.value();
      }
      if (auto parent = dynamic_cast<const Style *>(style)->m_parent) {
        return value(document, element, parent);
      }
      return {};
    }

  private:
    const char *m_property_class_name;
    const char *m_attribute_name;
  };

  class DirectionalProperty : public abstract::Style::DirectionalProperty {
  public:
    class Property : public abstract::Property {
    public:
      Property(const char *property_class_name, const char *attribute_name,
               const char *default_attribute_name)
          : m_attribute{property_class_name, attribute_name},
            m_default_attribute{property_class_name, default_attribute_name} {}

      [[nodiscard]] std::optional<std::string>
      value(const abstract::Document *document,
            const abstract::Element *element,
            const abstract::Style *style) const final {
        if (auto value = m_attribute.value(document, element, style)) {
          return value;
        }
        if (auto value = m_default_attribute.value(document, element, style)) {
          return value;
        }
        return {};
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

    [[nodiscard]] abstract::Property *right(const abstract::Document *,
                                            const abstract::Style *) final {
      return &m_right;
    }

    [[nodiscard]] abstract::Property *top(const abstract::Document *,
                                          const abstract::Style *) final {
      return &m_top;
    }

    [[nodiscard]] abstract::Property *left(const abstract::Document *,
                                           const abstract::Style *) final {
      return &m_left;
    }

    [[nodiscard]] abstract::Property *bottom(const abstract::Document *,
                                             const abstract::Style *) final {
      return &m_bottom;
    }

  private:
    Property m_right;
    Property m_top;
    Property m_left;
    Property m_bottom;
  };

  class Text final : public abstract::Style::Text {
  public:
    [[nodiscard]] abstract::Property *font_name(const abstract::Document *,
                                                const abstract::Style *) final {
      static StringAttributeProperty font_name{"style:text-properties",
                                               "style:font-name"};
      return &font_name;
    }

    [[nodiscard]] abstract::Property *font_size(const abstract::Document *,
                                                const abstract::Style *) final {
      static StringAttributeProperty font_size{"style:text-properties",
                                               "fo:font-size"};
      return &font_size;
    }

    [[nodiscard]] abstract::Property *
    font_weight(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty font_weight{"style:text-properties",
                                                 "fo:font-weight"};
      return &font_weight;
    }

    [[nodiscard]] abstract::Property *
    font_style(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty font_style{"style:text-properties",
                                                "fo:font-style"};
      return &font_style;
    }

    [[nodiscard]] abstract::Property *
    font_underline(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty font_underline{
          "style:text-properties", "style:text-underline-style"};
      return &font_underline;
    }

    [[nodiscard]] abstract::Property *
    font_line_through(const abstract::Document *,
                      const abstract::Style *) final {
      static StringAttributeProperty font_line_through{
          "style:text-properties", "style:text-line-through-style"};
      return &font_line_through;
    }

    [[nodiscard]] abstract::Property *
    font_shadow(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty font_shadow{"style:text-properties",
                                                 "fo:text-shadow"};
      return &font_shadow;
    }

    [[nodiscard]] abstract::Property *
    font_color(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty font_color{"style:text-properties",
                                                "fo:color"};
      return &font_color;
    }

    [[nodiscard]] abstract::Property *
    background_color(const abstract::Document *,
                     const abstract::Style *) final {
      static StringAttributeProperty background_color{"style:text-properties",
                                                      "fo:background-color"};
      return &background_color;
    }
  };

  class Paragraph final : public abstract::Style::Paragraph {
  public:
    [[nodiscard]] abstract::Property *
    text_align(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty text_align{"style:paragraph-properties",
                                                "fo:text-align"};
      return &text_align;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    margin(const abstract::Document *, const abstract::Style *) final {
      static DirectionalProperty margin{"style:paragraph-properties",
                                        "fo:margin",
                                        "fo:margin-right",
                                        "fo:margin-top",
                                        "fo:margin-left",
                                        "fo:margin-bottom"};
      return &margin;
    }
  };

  class Table final : public abstract::Style::Table {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      static StringAttributeProperty width{"style:table-properties",
                                           "style:width"};
      return &width;
    }
  };

  class TableColumn final : public abstract::Style::TableColumn {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      static StringAttributeProperty width{"style:table-column-properties",
                                           "style:column-width"};
      return &width;
    }
  };

  class TableRow final : public abstract::Style::TableRow {
  public:
    [[nodiscard]] abstract::Property *height(const abstract::Document *,
                                             const abstract::Style *) final {
      static StringAttributeProperty height{"style:table-row-properties",
                                            "style:row-height"};
      return &height;
    }
  };

  class TableCell final : public abstract::Style::TableCell {
  public:
    [[nodiscard]] abstract::Property *
    vertical_align(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty vertical_align{
          "style:table-cell-properties", "style:vertical-align"};
      return &vertical_align;
    }

    [[nodiscard]] abstract::Property *
    background_color(const abstract::Document *,
                     const abstract::Style *) final {
      static StringAttributeProperty background_color{
          "style:table-cell-properties", "fo:background-color"};
      return &background_color;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    padding(const abstract::Document *, const abstract::Style *) final {
      static DirectionalProperty padding{"style:table-cell-properties",
                                         "fo:padding",
                                         "fo:padding-right",
                                         "fo:padding-top",
                                         "fo:padding-left",
                                         "fo:padding-bottom"};
      return &padding;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    border(const abstract::Document *, const abstract::Style *) final {
      static DirectionalProperty border{"style:table-cell-properties",
                                        "fo:border",
                                        "fo:border-right",
                                        "fo:border-top",
                                        "fo:border-left",
                                        "fo:border-bottom"};
      return &border;
    }
  };

  class Graphic final : public abstract::Style::Graphic {
  public:
    [[nodiscard]] abstract::Property *
    stroke_width(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty stroke_width{"style:graphic-properties",
                                                  "svg:stroke-width"};
      return &stroke_width;
    }

    [[nodiscard]] abstract::Property *
    stroke_color(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty stroke_color{"style:graphic-properties",
                                                  "svg:stroke-color"};
      return &stroke_color;
    }

    [[nodiscard]] abstract::Property *
    fill_color(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty fill_color{"style:graphic-properties",
                                                "draw:fill-color"};
      return &fill_color;
    }

    [[nodiscard]] abstract::Property *
    vertical_align(const abstract::Document *, const abstract::Style *) final {
      static StringAttributeProperty vertical_align{
          "style:graphic-properties", "draw:textarea-vertical-align"};
      return &vertical_align;
    }
  };

  class PageLayout final : public abstract::Style::PageLayout {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      static StringAttributeProperty width{"style:page-layout-properties",
                                           "fo:page-width"};
      return &width;
    }

    [[nodiscard]] abstract::Property *height(const abstract::Document *,
                                             const abstract::Style *) final {
      static StringAttributeProperty height{"style:page-layout-properties",
                                            "fo:page-height"};
      return &height;
    }

    [[nodiscard]] abstract::Property *
    print_orientation(const abstract::Document *,
                      const abstract::Style *) final {
      static StringAttributeProperty print_orientation{
          "style:page-layout-properties", "style:print-orientation"};
      return &print_orientation;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    margin(const abstract::Document *, const abstract::Style *) final {
      static DirectionalProperty margin{"style:page-layout-properties",
                                        "fo:margin",
                                        "fo:margin-right",
                                        "fo:margin-top",
                                        "fo:margin-left",
                                        "fo:margin-bottom"};
      return &margin;
    }
  };

  [[nodiscard]] Text *text(const abstract::Document *) final {
    static Text text;
    return &text;
  }

  [[nodiscard]] virtual Paragraph *paragraph(const abstract::Document *) final {
    static Paragraph paragraph;
    return &paragraph;
  }

  [[nodiscard]] virtual Table *table(const abstract::Document *) final {
    static Table table;
    return &table;
  }

  [[nodiscard]] virtual TableColumn *
  table_column(const abstract::Document *) final {
    static TableColumn table_column;
    return &table_column;
  }

  [[nodiscard]] virtual TableRow *table_row(const abstract::Document *) final {
    static TableRow table_row;
    return &table_row;
  }

  [[nodiscard]] virtual TableCell *
  table_cell(const abstract::Document *) final {
    static TableCell table_cell;
    return &table_cell;
  }

  [[nodiscard]] virtual Graphic *graphic(const abstract::Document *) final {
    static Graphic graphic;
    return &graphic;
  }

  [[nodiscard]] virtual PageLayout *
  page_layout(const abstract::Document *) final {
    static PageLayout page_layout;
    return &page_layout;
  }

private:
  const Style *m_parent;

  pugi::xml_node m_node;
};

} // namespace

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_();
}

abstract::Style *StyleRegistry::style(const std::string &name) const {
  if (auto styles_it = m_styles.find(name); styles_it != std::end(m_styles)) {
    return styles_it->second.get();
  }
  return {};
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

abstract::Style *StyleRegistry::generate_style_(const std::string &name,
                                                const pugi::xml_node node) {
  auto &&style = m_styles[name];
  if (style) {
    return style.get();
  }

  abstract::Style *parent{nullptr};

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    if (auto parent_style_it = m_index.find(parent_attr.value());
        parent_style_it != std::end(m_index)) {
      parent = generate_style_(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  }

  style = std::make_unique<Style>(dynamic_cast<Style *>(parent), node);
  return style.get();
}

} // namespace odr::internal::ooxml::text
