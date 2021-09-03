#include <functional>
#include <internal/abstract/document.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/text/ooxml_text_element.h>
#include <internal/ooxml/text/ooxml_text_style.h>
#include <internal/util/property_util.h>

namespace odr::internal::ooxml::text {

class Style : public abstract::Style {
public:
  Style(const Style *parent, pugi::xml_node node)
      : m_parent{parent}, m_node{node} {}

  class PropertyFunction : public abstract::Property {
  public:
    using Function = std::function<std::optional<std::string>(
        const Style *style, pugi::xml_node node)>;

    explicit PropertyFunction(Function function) : m_function{function} {}

    [[nodiscard]] std::optional<std::string>
    value(const abstract::Document *document,
          const abstract::Element *abstract_element,
          const abstract::Style *abstract_style,
          const StyleContext style_context) const final {
      auto style = static_cast<const Style *>(abstract_style);
      if (auto element = static_cast<const Element *>(abstract_element)) {
        auto result = m_function(style, element->m_node);
        if (result) {
          return result;
        }
      }
      if (style->m_node) {
        auto result = m_function(style, style->m_node);
        if (result) {
          return result;
        }
      }
      if (auto parent = style->m_parent) {
        return value(document, abstract_element, parent, style_context);
      }
      return {};
    }

  private:
    Function m_function;
  };

  class Text final : public abstract::Style::Text {
  public:
    [[nodiscard]] abstract::Property *font_name(const abstract::Document *,
                                                const abstract::Style *) final {
      static PropertyFunction font_name{[](const Style *, pugi::xml_node node) {
        return read_optional_attribute(
            node.child("w:rPr").child("w:rFonts").attribute("w:ascii"));
      }};
      return &font_name;
    }

    [[nodiscard]] abstract::Property *font_size(const abstract::Document *,
                                                const abstract::Style *) final {
      static PropertyFunction font_name{[](const Style *, pugi::xml_node node) {
        return read_optional_attribute(
            node.child("w:rPr").child("w:sz").attribute("w:val"),
            read_half_point_attribute);
      }};
      return &font_name;
    }

    [[nodiscard]] abstract::Property *
    font_weight(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction font_weight{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:rPr").child("w:color").attribute("w:val"),
                read_color_attribute);
          }};
      return &font_weight;
    }

    [[nodiscard]] abstract::Property *
    font_style(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction font_style{
          [](const Style *, pugi::xml_node node) {
            return read_optional_node(
                node.child("w:rPr").child("w:i"),
                [](const pugi::xml_node) { return "italic"; });
          }};
      return &font_style;
    }

    [[nodiscard]] abstract::Property *
    font_underline(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction font_underline{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:rPr").child("w:u").attribute("w:val"),
                read_line_attribute);
          }};
      return &font_underline;
    }

    [[nodiscard]] abstract::Property *
    font_line_through(const abstract::Document *,
                      const abstract::Style *) final {
      static PropertyFunction font_line_through{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:rPr").child("w:strike").attribute("w:val"),
                read_line_attribute);
          }};
      return &font_line_through;
    }

    [[nodiscard]] abstract::Property *
    font_shadow(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction font_shadow{
          [](const Style *, pugi::xml_node node) {
            return read_optional_node(node.child("w:rPr").child("w:shadow"),
                                      read_shadow_attribute);
          }};
      return &font_shadow;
    }

    [[nodiscard]] abstract::Property *
    font_color(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction font_color{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:rPr").child("w:color").attribute("w:val"),
                read_color_attribute);
          }};
      return &font_color;
    }

    [[nodiscard]] abstract::Property *
    background_color(const abstract::Document *,
                     const abstract::Style *) final {
      static PropertyFunction font_color{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:rPr").child("w:highlight").attribute("w:val"),
                read_color_attribute);
          }};
      return &font_color;
    }
  };

  class Paragraph final : public abstract::Style::Paragraph {
  public:
    [[nodiscard]] abstract::Property *
    text_align(const abstract::Document *, const abstract::Style *) final {
      static PropertyFunction text_align{
          [](const Style *, pugi::xml_node node) {
            return read_optional_attribute(
                node.child("w:pPr").child("w:jc").attribute("w:val"));
          }};
      return &text_align;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    margin(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }
  };

  class Table final : public abstract::Style::Table {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      return nullptr;
    }
  };

  class TableColumn final : public abstract::Style::TableColumn {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      return nullptr;
    }
  };

  class TableRow final : public abstract::Style::TableRow {
  public:
    [[nodiscard]] abstract::Property *height(const abstract::Document *,
                                             const abstract::Style *) final {
      return nullptr;
    }
  };

  class TableCell final : public abstract::Style::TableCell {
  public:
    [[nodiscard]] abstract::Property *
    vertical_align(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *
    background_color(const abstract::Document *,
                     const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    padding(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    border(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }
  };

  class Graphic final : public abstract::Style::Graphic {
  public:
    [[nodiscard]] abstract::Property *
    stroke_width(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *
    stroke_color(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *
    fill_color(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *
    vertical_align(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
    }
  };

  class PageLayout final : public abstract::Style::PageLayout {
  public:
    [[nodiscard]] abstract::Property *width(const abstract::Document *,
                                            const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *height(const abstract::Document *,
                                             const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Property *
    print_orientation(const abstract::Document *,
                      const abstract::Style *) final {
      return nullptr;
    }

    [[nodiscard]] abstract::Style::DirectionalProperty *
    margin(const abstract::Document *, const abstract::Style *) final {
      return nullptr;
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

StyleRegistry::StyleRegistry() = default;

StyleRegistry::StyleRegistry(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_();
}

abstract::Style *StyleRegistry::style(const std::string &name) const {
  if (auto styles_it = m_styles.find(name); styles_it != std::end(m_styles)) {
    return styles_it->second.get();
  }
  return nullptr;
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

  if (auto parent_attr = node.child("w:basedOn").attribute("w:val");
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
