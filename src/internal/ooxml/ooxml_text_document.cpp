#include <internal/common/path.h>
#include <internal/ooxml/ooxml_text_document.h>
#include <internal/util/map_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

class OfficeOpenXmlTextDocument::PropertyRegistry final {
public:
  using Get = std::function<std::any(pugi::xml_node node)>;

  static PropertyRegistry &instance() {
    static PropertyRegistry instance;
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
      auto value = p.second.get(node);
      if (value.has_value()) {
        result[p.first] = value;
      }
    }
  }

private:
  struct Entry {
    Get get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  PropertyRegistry() {
    register_(ElementType::TEXT, ElementProperty::TEXT,
              [](const pugi::xml_node node) {
                std::string name = node.name();
                if (name == "w:tab") {
                  return "\t";
                }
                return node.first_child().text().as_string();
              });

    default_register_(ElementType::BOOKMARK, ElementProperty::NAME, "w:name");

    register_link_();
  }

  void register_(const ElementType element, const ElementProperty property,
                 Get get) {
    m_registry[element][property].get = std::move(get);
  }

  void default_register_(const ElementType element,
                         const ElementProperty property,
                         const char *attribute_name) {
    register_(element, property, [attribute_name](const pugi::xml_node node) {
      auto attribute = node.attribute(attribute_name);
      if (!attribute) {
        return std::any();
      }
      return std::any(attribute.value());
    });
  }

  void register_link_() {
    register_(ElementType::LINK, ElementProperty::HREF,
              [](const pugi::xml_node node) {
                if (auto anchor = node.attribute("w:anchor")) {
                  return std::any(anchor.value());
                }
                auto id = node.attribute("r:id");
                // TODO
                return std::any("");
              });
  }
};

namespace {
class StylePropertyRegistry {
public:
  using Get = std::function<std::any(pugi::xml_node node)>;

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
      auto value = p.second.get(node);
      if (value.has_value()) {
        result[p.first] = value;
      }
    }
  }

private:
  struct Entry {
    Get get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  StylePropertyRegistry() {
    register_text_(ElementType::PARAGRAPH);
    register_paragraph_(ElementType::PARAGRAPH);

    register_text_(ElementType::SPAN);
  }

  void register_get_(const ElementType element, const ElementProperty property,
                     Get get) {
    m_registry[element][property].get = std::move(get);
  }

  void default_register_get_(const ElementType element,
                             const ElementProperty property,
                             const char *property_class_element,
                             const char *property_element,
                             const char *attribute_name = "w:val") {
    m_registry[element][property].get =
        [property_class_element, property_element,
         attribute_name](const pugi::xml_node node) {
          auto attribute = node.child(property_class_element)
                               .child(property_element)
                               .attribute(attribute_name);
          if (!attribute) {
            return std::any();
          }
          return std::any(attribute.value());
        };
  }

  void register_color_(const ElementType element,
                       const ElementProperty property,
                       const char *property_class_element,
                       const char *property_element) {
    m_registry[element][property].get =
        [property_class_element, property_element](const pugi::xml_node node) {
          auto attribute = node.child(property_class_element)
                               .child(property_element)
                               .attribute("w:val");
          if (!attribute) {
            return std::any();
          }
          std::string value = attribute.value();
          if (value == "auto") {
            return std::any();
          }
          if (value.length() == 6) {
            return std::any("#" + value);
          }
          return std::any(attribute.value());
        };
  }

  void register_half_point_(const ElementType element,
                            const ElementProperty property,
                            const char *property_class_element,
                            const char *property_element) {
    m_registry[element][property].get =
        [property_class_element, property_element](const pugi::xml_node node) {
          auto attribute = node.child(property_class_element)
                               .child(property_element)
                               .attribute("w:val");
          if (!attribute) {
            return std::any();
          }
          return std::any(std::to_string(attribute.as_int() * 0.5f) + "pt");
        };
  }

  void register_paragraph_(const ElementType element) {
    const auto paragraph_properties = "w:pPr";

    default_register_get_(element, ElementProperty::TEXT_ALIGN,
                          paragraph_properties, "w:jc");
  }

  void register_text_(const ElementType element) {
    const auto run_properties = "w:rPr";

    default_register_get_(element, ElementProperty::FONT_NAME, run_properties,
                          "w:rFonts", "w:ascii");
    register_half_point_(element, ElementProperty::FONT_SIZE, run_properties,
                         "w:sz");
    register_color_(element, ElementProperty::FONT_COLOR, run_properties,
                    "w:color");
    register_color_(element, ElementProperty::BACKGROUND_COLOR, run_properties,
                    "w:highlight");
    register_get_(element, ElementProperty::FONT_WEIGHT,
                  [run_properties](const pugi::xml_node node) {
                    auto property = node.child(run_properties).child("w:b");
                    if (!property) {
                      return std::any();
                    }
                    return std::any("bold");
                  });
    register_get_(element, ElementProperty::FONT_STYLE,
                  [run_properties](const pugi::xml_node node) {
                    auto property = node.child(run_properties).child("w:i");
                    if (!property) {
                      return std::any();
                    }
                    return std::any("italic");
                  });
    register_get_(element, ElementProperty::FONT_UNDERLINE,
                  [run_properties](const pugi::xml_node node) {
                    auto property = node.child(run_properties).child("w:u");
                    if (!property || std::string(property.value()) == "none") {
                      return std::any();
                    }
                    return std::any("solid");
                  });
    register_get_(element, ElementProperty::FONT_STRIKETHROUGH,
                  [run_properties](const pugi::xml_node node) {
                    auto property =
                        node.child(run_properties).child("w:strike");
                    if (!property) {
                      return std::any();
                    }
                    return std::any("solid");
                  });
    register_get_(element, ElementProperty::FONT_SHADOW,
                  [run_properties](const pugi::xml_node node) {
                    auto property =
                        node.child(run_properties).child("w:shadow");
                    if (!property) {
                      return std::any();
                    }
                    return std::any("1pt 1pt");
                  });
  }
};
} // namespace

OfficeOpenXmlTextDocument::Style::Style() = default;

OfficeOpenXmlTextDocument::Style::Style(const pugi::xml_node styles_root) {
  generate_indices_(styles_root);
  generate_styles_();
}

[[nodiscard]] std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlTextDocument::Style::resolve_style(
    const ElementType element_type, const pugi::xml_node element) const {
  std::unordered_map<ElementProperty, std::any> result;

  pugi::xml_node properties;
  std::string style_name;

  // TODO map
  if (element_type == ElementType::PARAGRAPH) {
    properties = element.child("w:pPr");
    style_name = properties.child("w:pStyle").attribute("w:val").as_string();
  } else if (element_type == ElementType::SPAN) {
    properties = element.child("w:rPr");
    style_name = properties.child("w:rStyle").attribute("w:val").as_string();
  }

  if (!style_name.empty()) {
    result[ElementProperty::STYLE_NAME] = style_name;
  }

  auto style_it = m_styles.find(style_name);
  if (style_it != std::end(m_styles)) {
    style_it->second->properties(element_type, result);
  }

  StylePropertyRegistry::instance().resolve_properties(element_type, element,
                                                       result);

  return result;
}

OfficeOpenXmlTextDocument::Style::Entry::Entry(std::shared_ptr<Entry> parent,
                                               pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

void OfficeOpenXmlTextDocument::Style::Entry::properties(
    const ElementType element,
    std::unordered_map<ElementProperty, std::any> &result) const {
  if (m_parent) {
    m_parent->properties(element, result);
  }

  StylePropertyRegistry::instance().resolve_properties(element, m_node, result);
}

void OfficeOpenXmlTextDocument::Style::generate_indices_(
    const pugi::xml_node styles_root) {
  for (auto style : styles_root) {
    std::string element_name = style.name();

    if (element_name == "w:style") {
      m_index[style.attribute("w:styleId").value()] = style;
    }
  }
}

void OfficeOpenXmlTextDocument::Style::generate_styles_() {
  for (auto &&e : m_index) {
    generate_style_(e.first, e.second);
  }
}

std::shared_ptr<OfficeOpenXmlTextDocument::Style::Entry>
OfficeOpenXmlTextDocument::Style::generate_style_(const std::string &name,
                                                  const pugi::xml_node node) {
  if (auto style_it = m_styles.find(name); style_it != std::end(m_styles)) {
    return style_it->second;
  }

  std::shared_ptr<Entry> parent;

  // TODO use defaults as parent?

  if (auto parent_attr = node.child("w:basedOn").attribute("w:val");
      parent_attr) {
    if (auto parent_style_it = m_index.find(parent_attr.value());
        parent_style_it != std::end(m_index)) {
      parent = generate_style_(parent_attr.value(), parent_style_it->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Entry>(parent, node);
}

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  m_style = Style(m_styles_xml.document_element());

  m_root =
      register_element_(m_document_xml.document_element().first_child(), 0, 0);

  if (!m_root) {
    throw NoOpenDocumentFile();
  }
}

ElementIdentifier OfficeOpenXmlTextDocument::register_element_(
    const pugi::xml_node node, const ElementIdentifier parent,
    const ElementIdentifier previous_sibling) {
  static std::unordered_map<std::string, ElementType> element_type_table{
      {"w:body", ElementType::ROOT},
      {"w:t", ElementType::TEXT},
      {"w:tab", ElementType::TEXT},
      {"w:p", ElementType::PARAGRAPH},
      {"w:r", ElementType::SPAN},
      {"w:bookmarkStart", ElementType::BOOKMARK},
      {"w:hyperlink", ElementType::LINK},
  };

  if (!node) {
    return {};
  }

  ElementType element_type = ElementType::NONE;

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    util::map::lookup_map(element_type_table, element, element_type);
  }

  if (element_type == ElementType::NONE) {
    // TODO log node
    return {};
  }

  auto new_element = new_element_(node, element_type, parent, previous_sibling);

  register_children_(node, new_element, {});

  return new_element;
}

std::pair<ElementIdentifier, ElementIdentifier>
OfficeOpenXmlTextDocument::register_children_(
    const pugi::xml_node node, const ElementIdentifier parent,
    ElementIdentifier previous_sibling) {
  ElementIdentifier first_element;

  for (auto &&child_node : node) {
    const ElementIdentifier child =
        register_element_(child_node, parent, previous_sibling);
    if (!child) {
      continue;
    }
    if (!first_element) {
      first_element = child;
    }
    previous_sibling = child;
  }

  return {first_element, previous_sibling};
}

ElementIdentifier OfficeOpenXmlTextDocument::new_element_(
    const pugi::xml_node node, const ElementType type,
    const ElementIdentifier parent, const ElementIdentifier previous_sibling) {
  Element element;
  element.node = node;
  element.type = type;
  element.parent = parent;
  element.previous_sibling = previous_sibling;

  m_elements.push_back(element);
  ElementIdentifier result = m_elements.size();

  if (parent && !previous_sibling) {
    element_(parent)->first_child = result;
  }
  if (previous_sibling) {
    element_(previous_sibling)->next_sibling = result;
  }

  return result;
}

OfficeOpenXmlTextDocument::Element *
OfficeOpenXmlTextDocument::element_(const ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

const OfficeOpenXmlTextDocument::Element *
OfficeOpenXmlTextDocument::element_(const ElementIdentifier element_id) const {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

bool OfficeOpenXmlTextDocument::editable() const noexcept { return false; }

bool OfficeOpenXmlTextDocument::savable(const bool encrypted) const noexcept {
  return false;
}

void OfficeOpenXmlTextDocument::save(const common::Path &path) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlTextDocument::save(const common::Path &path,
                                     const char *password) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlTextDocument::document_type() const noexcept {
  return DocumentType::TEXT;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlTextDocument::files() const noexcept {
  return m_filesystem;
}

ElementIdentifier OfficeOpenXmlTextDocument::root_element() const {
  return m_root;
}

ElementIdentifier OfficeOpenXmlTextDocument::first_entry_element() const {
  return element_first_child(m_root);
}

ElementType OfficeOpenXmlTextDocument::element_type(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return ElementType::NONE;
  }
  return element_(element_id)->type;
}

ElementIdentifier OfficeOpenXmlTextDocument::element_parent(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->parent;
}

ElementIdentifier OfficeOpenXmlTextDocument::element_first_child(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->first_child;
}

ElementIdentifier OfficeOpenXmlTextDocument::element_previous_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->previous_sibling;
}

ElementIdentifier OfficeOpenXmlTextDocument::element_next_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->next_sibling;
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlTextDocument::element_properties(
    ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  PropertyRegistry::instance().resolve_properties(element->type, element->node,
                                                  result);

  auto style_properties = m_style.resolve_style(element->type, element->node);
  result.insert(std::begin(style_properties), std::end(style_properties));

  return result;
}

void OfficeOpenXmlTextDocument::update_element_properties(
    const ElementIdentifier element_id,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OfficeOpenXmlTextDocument::table(const ElementIdentifier element_id) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
