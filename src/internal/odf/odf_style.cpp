#include <functional>
#include <internal/odf/odf_style.h>
#include <internal/util/map_util.h>
#include <internal/util/string_util.h>
#include <odr/document.h>

namespace odr::internal::odf {

namespace {
class StylePropertyRegistry {
public:
  static StylePropertyRegistry &instance() {
    static StylePropertyRegistry instance;
    return instance;
  }

  void
  resolve_properties(const ElementType element, const pugi::xml_node node,
                     std::unordered_map<ElementProperty, std::any> &result) {
    auto it = m_registry.find(element);
    if (it == std::end(m_registry)) {
      return;
    }
    for (auto &&p : it->second) {
      auto value = p.second.get(
          node, util::map::lookup_map_default(result, p.first, std::any()));
      if (value.has_value()) {
        result[p.first] = value;
      }
    }
  }

private:
  struct Entry {
    std::function<std::any(pugi::xml_node node, std::any previous_value)> get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  StylePropertyRegistry() {
    register_text_(ElementType::PARAGRAPH);
    register_paragraph_(ElementType::PARAGRAPH);

    register_text_(ElementType::SPAN);

    register_text_(ElementType::LINK);

    default_register_(ElementType::TABLE, ElementProperty::WIDTH,
                      "style:table-properties", "style:width");

    register_table_column_();

    register_table_row_();

    register_text_(ElementType::TABLE_CELL);
    register_paragraph_(ElementType::TABLE_CELL);
    register_table_cell_();

    register_graphic_(ElementType::RECT);
    register_graphic_(ElementType::LINE);
    register_graphic_(ElementType::CIRCLE);

    register_page_layout_(ElementType::ROOT);
    register_page_layout_(ElementType::SLIDE);
    register_page_layout_(ElementType::PAGE);
  }

  void default_register_get_(const ElementType element,
                             const ElementProperty property,
                             const char *property_class_name,
                             const char *attribute_name) {
    m_registry[element][property].get =
        [property_class_name, attribute_name](const pugi::xml_node node,
                                              std::any previous_value) {
          auto attribute =
              node.child(property_class_name).attribute(attribute_name);
          if (!attribute) {
            return previous_value;
          }
          return std::any(attribute.value());
        };
  }

  void default_register_(const ElementType element,
                         const ElementProperty property,
                         const char *property_class_name,
                         const char *attribute_name) {
    default_register_get_(element, property, property_class_name,
                          attribute_name);
  }

  void default_register_directions_(
      const ElementType element, const ElementProperty top_property,
      const ElementProperty bottom_property,
      const ElementProperty left_property, const ElementProperty right_property,
      const char *property_class_name, const char *attribute_base_name,
      const char *attribute_top_name, const char *attribute_bottom_name,
      const char *attribute_left_name, const char *attribute_right_name) {
    auto get_factory = [property_class_name, attribute_base_name](
                           const char *attribute_direction_name) {
      return [property_class_name, attribute_base_name,
              attribute_direction_name](const pugi::xml_node node,
                                        std::any previous_value) {
        auto property_class = node.child(property_class_name);
        if (auto attribute = property_class.attribute(attribute_direction_name);
            attribute) {
          return std::any(attribute.value());
        }
        if (auto attribute = property_class.attribute(attribute_base_name);
            attribute) {
          return std::any(attribute.value());
        }
        return previous_value;
      };
    };

    m_registry[element][top_property].get = get_factory(attribute_top_name);
    m_registry[element][bottom_property].get =
        get_factory(attribute_bottom_name);
    m_registry[element][left_property].get = get_factory(attribute_left_name);
    m_registry[element][right_property].get = get_factory(attribute_right_name);
  }

  void register_paragraph_(const ElementType element) {
    static auto property_class_name = "style:paragraph-properties";
    default_register_(element, ElementProperty::TEXT_ALIGN, property_class_name,
                      "fo:text-align");
    default_register_directions_(
        element, ElementProperty::MARGIN_TOP, ElementProperty::MARGIN_BOTTOM,
        ElementProperty::MARGIN_LEFT, ElementProperty::MARGIN_RIGHT,
        property_class_name, "fo:margin", "fo:margin-top", "fo:margin-bottom",
        "fo:margin-left", "fo:margin-right");
  }

  void register_text_(const ElementType element) {
    static auto property_class_name = "style:text-properties";
    default_register_(element, ElementProperty::FONT_NAME, property_class_name,
                      "style:font-name");
    default_register_(element, ElementProperty::FONT_SIZE, property_class_name,
                      "fo:font-size");
    default_register_(element, ElementProperty::FONT_WEIGHT,
                      property_class_name, "fo:font-weight");
    default_register_(element, ElementProperty::FONT_STYLE, property_class_name,
                      "fo:font-style");
    default_register_(element, ElementProperty::FONT_UNDERLINE,
                      property_class_name, "style:text-underline-style");
    default_register_(element, ElementProperty::FONT_STRIKETHROUGH,
                      property_class_name, "style:text-line-through-style");
    default_register_(element, ElementProperty::FONT_SHADOW,
                      property_class_name, "fo:text-shadow");
    default_register_(element, ElementProperty::FONT_COLOR, property_class_name,
                      "fo:color");
    default_register_(element, ElementProperty::BACKGROUND_COLOR,
                      property_class_name, "fo:background-color");
  }

  void register_table_column_() {
    static auto property_class_name = "style:table-column-properties";
    default_register_(ElementType::TABLE_COLUMN, ElementProperty::WIDTH,
                      property_class_name, "style:column-width");
  }

  void register_table_row_() {
    static auto property_class_name = "style:table-row-properties";
    default_register_(ElementType::TABLE_ROW, ElementProperty::HEIGHT,
                      property_class_name, "style:row-height");
  }

  void register_table_cell_() {
    static auto property_class_name = "style:table-cell-properties";
    default_register_(ElementType::TABLE_CELL, ElementProperty::VERTICAL_ALIGN,
                      property_class_name, "style:vertical-align");
    default_register_(ElementType::TABLE_CELL,
                      ElementProperty::TABLE_CELL_BACKGROUND_COLOR,
                      property_class_name, "fo:background-color");
    default_register_directions_(
        ElementType::TABLE_CELL, ElementProperty::PADDING_TOP,
        ElementProperty::PADDING_BOTTOM, ElementProperty::PADDING_LEFT,
        ElementProperty::PADDING_RIGHT, property_class_name, "fo:padding",
        "fo:padding-top", "fo:padding-bottom", "fo:padding-left",
        "fo:padding-right");
    default_register_directions_(
        ElementType::TABLE_CELL, ElementProperty::BORDER_TOP,
        ElementProperty::BORDER_BOTTOM, ElementProperty::BORDER_LEFT,
        ElementProperty::BORDER_RIGHT, property_class_name, "fo:border",
        "fo:border-top", "fo:border-bottom", "fo:border-left",
        "fo:border-right");
  }

  void register_graphic_(const ElementType element) {
    static auto property_class_name = "style:graphic-properties";
    default_register_(element, ElementProperty::STROKE_WIDTH,
                      property_class_name, "svg:stroke-width");
    default_register_(element, ElementProperty::STROKE_COLOR,
                      property_class_name, "svg:stroke-color");
    default_register_(element, ElementProperty::FILL_COLOR, property_class_name,
                      "draw:fill-color");
    default_register_(element, ElementProperty::VERTICAL_ALIGN,
                      property_class_name, "draw:textarea-vertical-align");
  }

  void register_page_layout_(const ElementType element) {
    static auto property_class_name = "style:page-layout-properties";
    default_register_directions_(
        element, ElementProperty::MARGIN_TOP, ElementProperty::MARGIN_BOTTOM,
        ElementProperty::MARGIN_LEFT, ElementProperty::MARGIN_RIGHT,
        property_class_name, "fo:margin", "fo:margin-top", "fo:margin-bottom",
        "fo:margin-left", "fo:margin-right");
    default_register_(element, ElementProperty::WIDTH, property_class_name,
                      "fo:page-width");
    default_register_(element, ElementProperty::HEIGHT, property_class_name,
                      "fo:page-height");
    default_register_(element, ElementProperty::PRINT_ORIENTATION,
                      property_class_name, "style:print-orientation");
  }
};
} // namespace

Style::Entry::Entry(std::shared_ptr<Entry> parent, pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
Style::Entry::properties(const ElementType element) const {
  std::unordered_map<ElementProperty, std::any> result;

  if (m_parent) {
    result = m_parent->properties(element);
  }

  StylePropertyRegistry::instance().resolve_properties(element, m_node, result);

  return result;
}

Style::Style() = default;

Style::Style(const pugi::xml_node content_root,
             const pugi::xml_node styles_root) {
  generate_indices_(content_root, styles_root);
  generate_styles_();
}

void Style::generate_indices_(const pugi::xml_node content_root,
                              const pugi::xml_node styles_root) {
  if (auto font_face_decls = styles_root.child("office:font-face-decls");
      font_face_decls) {
    generate_indices_(font_face_decls);
  }

  if (auto styles = styles_root.child("office:styles"); styles) {
    generate_indices_(styles);
  }

  if (auto automatic_styles = styles_root.child("office:automatic-styles");
      automatic_styles) {
    generate_indices_(automatic_styles);
  }

  if (auto master_styles = styles_root.child("office:master-styles");
      master_styles) {
    generate_indices_(master_styles);
  }

  // content styles

  if (auto font_face_decls = content_root.child("office:font-face-decls");
      font_face_decls) {
    generate_indices_(font_face_decls);
  }

  if (auto automatic_styles = content_root.child("office:automatic-styles");
      automatic_styles) {
    generate_indices_(automatic_styles);
  }
}

void Style::generate_indices_(const pugi::xml_node node) {
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

void Style::generate_styles_() {
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

std::shared_ptr<Style::Entry>
Style::generate_default_style_(const std::string &name,
                               const pugi::xml_node node) {
  if (auto it = m_default_styles.find(name); it != std::end(m_default_styles)) {
    return it->second;
  }

  return m_default_styles[name] = std::make_shared<Entry>(nullptr, node);
}

std::shared_ptr<Style::Entry>
Style::generate_style_(const std::string &name, const pugi::xml_node node) {
  if (auto style_it = m_styles.find(name); style_it != std::end(m_styles)) {
    return style_it->second;
  }

  std::shared_ptr<Entry> parent;

  if (auto parent_attr = node.attribute("style:parent-style-name");
      parent_attr) {
    if (auto parent_style_it = m_index_style.find(parent_attr.value());
        parent_style_it != std::end(m_index_style)) {
      parent = generate_style_(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  } else if (auto family_attr = node.attribute("style:family-name");
             family_attr) {
    if (auto family_style_it = m_index_default_style.find(name);
        family_style_it != std::end(m_index_default_style)) {
      parent =
          generate_default_style_(parent_attr.value(), family_style_it->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Entry>(parent, node);
}

std::shared_ptr<Style::Entry>
Style::generate_page_layout_(const std::string &name,
                             const pugi::xml_node node) {
  if (auto it = m_page_layouts.find(name); it != std::end(m_page_layouts)) {
    return it->second;
  }

  return m_page_layouts[name] = std::make_shared<Entry>(nullptr, node);
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_style(const ElementType element,
                     const std::string &style_name) const {
  auto style_it = m_styles.find(style_name);
  if (style_it == std::end(m_styles)) {
    return {};
  }
  return style_it->second->properties(element);
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_page_layout(const ElementType element,
                           const std::string &page_layout_name) const {
  auto page_layout_it = m_page_layouts.find(page_layout_name);
  if (page_layout_it == std::end(m_page_layouts)) {
    return {};
  }
  return page_layout_it->second->properties(element);
}

std::unordered_map<ElementProperty, std::any>
Style::resolve_master_page(const ElementType element,
                           const std::string &master_page_name) const {
  auto master_page_it = m_index_master_page.find(master_page_name);
  if (master_page_it == std::end(m_index_master_page)) {
    return {};
  }

  std::unordered_map<ElementProperty, std::any> result;
  if (auto page_layout_name_attr =
          master_page_it->second.attribute("style:page-layout-name")) {
    auto style = resolve_page_layout(element, page_layout_name_attr.value());
    result.insert(std::begin(style), std::end(style));
  }
  if (auto style_name_attr =
          master_page_it->second.attribute("draw:style-name")) {
    auto style = resolve_style(element, style_name_attr.value());
    result.insert(std::begin(style), std::end(style));
  }
  return result;
}

pugi::xml_node
Style::master_page_node(const std::string &master_page_name) const {
  auto master_page_it = m_index_master_page.find(master_page_name);
  if (master_page_it == std::end(m_index_master_page)) {
    return {};
  }
  return master_page_it->second;
}

std::optional<std::string> Style::first_master_page() const {
  return m_first_master_page;
}

} // namespace odr::internal::odf
