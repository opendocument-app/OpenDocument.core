#include <internal/ooxml/ooxml_document.h>
#include <internal/util/map_util.h>

namespace odr::internal::ooxml {

OfficeOpenXmlDocument::OfficeOpenXmlDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {}

ElementIdentifier OfficeOpenXmlDocument::register_element_(
    const pugi::xml_node node, const ElementIdentifier parent,
    const ElementIdentifier previous_sibling) {
  static std::unordered_map<std::string, ElementType> element_type_table{
      // TODO DrawingML
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
OfficeOpenXmlDocument::register_children_(const pugi::xml_node node,
                                          const ElementIdentifier parent,
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

ElementIdentifier OfficeOpenXmlDocument::new_element_(
    const pugi::xml_node node, const ElementType type,
    const ElementIdentifier parent, const ElementIdentifier previous_sibling) {
  // TODO
  auto result = Document::new_element_(type, nullptr, parent, previous_sibling);
  m_element_nodes[result] = node;
  return result;
}

pugi::xml_node
OfficeOpenXmlDocument::element_node_(const ElementIdentifier element_id) const {
  return m_element_nodes.at(element_id);
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlDocument::files() const noexcept {
  return m_filesystem;
}

} // namespace odr::internal::ooxml
