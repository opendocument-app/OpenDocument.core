#include <internal/common/path.h>
#include <internal/ooxml/ooxml_text_document.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/util/map_util.h>
#include <internal/util/property_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

namespace {
std::any read_bookmark_name_property(const pugi::xml_node node) {
  if (auto attribute = node.attribute("w:name")) {
    return attribute.value();
  }
  return {};
}

std::any read_link_href_property(const pugi::xml_node node) {
  if (auto anchor = node.attribute("w:anchor")) {
    return anchor.value();
  }
  // auto id = node.attribute("r:id");
  // TODO
  return "";
}

void resolve_text_element_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  util::property::set_optional_property(ElementProperty::TEXT,
                                        read_text_property(node), result);
}

void resolve_bookmark_element_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  util::property::set_optional_property(
      ElementProperty::NAME, read_bookmark_name_property(node), result);
}

void resolve_link_element_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  util::property::set_optional_property(ElementProperty::HREF,
                                        read_link_href_property(node), result);
}

void resolve_element_properties(
    const ElementType element, const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  if (element == ElementType::TEXT) {
    resolve_text_element_properties(node, result);
  } else if (element == ElementType::BOOKMARK) {
    resolve_bookmark_element_properties(node, result);
  } else if (element == ElementType::LINK) {
    resolve_link_element_properties(node, result);
  }
}
} // namespace

OfficeOpenXmlTextDocument::OfficeOpenXmlTextDocument(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  m_style = OfficeOpenXmlTextStyle(m_styles_xml.document_element());

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
  auto result = Document::new_element_(type, parent, previous_sibling);
  m_element_nodes[result] = node;
  return result;
}

pugi::xml_node OfficeOpenXmlTextDocument::element_node_(
    const ElementIdentifier element_id) const {
  return m_element_nodes.at(element_id);
}

bool OfficeOpenXmlTextDocument::editable() const noexcept { return false; }

bool OfficeOpenXmlTextDocument::savable(
    const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlTextDocument::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlTextDocument::save(const common::Path & /*path*/,
                                     const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlTextDocument::document_type() const noexcept {
  return DocumentType::TEXT;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlTextDocument::files() const noexcept {
  return m_filesystem;
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlTextDocument::element_properties(
    ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  auto element_node = element_node_(element_id);

  resolve_element_properties(element->type, element_node, result);

  auto style_properties = m_style.resolve_style(element->type, element_node);
  result.insert(std::begin(style_properties), std::end(style_properties));

  return result;
}

void OfficeOpenXmlTextDocument::update_element_properties(
    const ElementIdentifier /*element_id*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
