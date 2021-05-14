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
    const auto paragraph_properties = "w:pPr";

    register_(ElementType::TEXT, ElementProperty::TEXT,
              [](const pugi::xml_node node) {
                return node.first_child().text().as_string();
              });

    default_register_(ElementType::PARAGRAPH, ElementProperty::STYLE_NAME,
                      paragraph_properties, "w:pStyle");
    default_register_(ElementType::PARAGRAPH, ElementProperty::TEXT_ALIGN,
                      paragraph_properties, "w:jc");
  }

  void register_(const ElementType element, const ElementProperty property,
                 Get get) {
    m_registry[element][property].get = std::move(get);
  }

  void default_register_(const ElementType element,
                         const ElementProperty property,
                         const char *property_class_element,
                         const char *property_element) {
    register_(
        element, property,
        [property_class_element, property_element](const pugi::xml_node node) {
          auto attribute = node.child(property_class_element)
                               .child(property_element)
                               .attribute("w:val");
          if (!attribute) {
            return std::any();
          }
          return std::any(attribute.value());
        });
  }
};

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

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

bool OfficeOpenXmlTextDocument::savable(bool encrypted) const noexcept {
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

  return result;
}

void OfficeOpenXmlTextDocument::update_element_properties(
    ElementIdentifier element_id,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OfficeOpenXmlTextDocument::table(const ElementIdentifier element_id) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
