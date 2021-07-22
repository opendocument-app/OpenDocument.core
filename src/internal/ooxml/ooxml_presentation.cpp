#include <internal/common/path.h>
#include <internal/ooxml/ooxml_presentation.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/util/map_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

namespace {} // namespace

OfficeOpenXmlPresentation::OfficeOpenXmlPresentation(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  auto presentation_xml =
      util::xml::parse(*m_filesystem, "ppt/presentation.xml");

  m_root = new_element_({}, ElementType::ROOT, {}, {});

  m_slide_width = read_emus_attribute(
      presentation_xml.document_element().child("p:sldSz").attribute("cx"));
  m_slide_height = read_emus_attribute(
      presentation_xml.document_element().child("p:sldSz").attribute("cy"));

  auto relationships =
      parse_relationships(*m_filesystem, "ppt/presentation.xml");
  ElementIdentifier previous_sibling;
  for (auto slide :
       presentation_xml.document_element().children("p:sldIdLst")) {
    auto slide_rid = slide.attribute("r:id").value();
    auto slide_path = common::Path("ppt").join(relationships.at(slide_rid));
    auto slide_xml = util::xml::parse(*m_filesystem, slide_path);

    previous_sibling = register_element_(slide_xml.document_element(), m_root,
                                         previous_sibling);

    m_slides_xml.push_back(std::move(slide_xml));
  }
}

ElementIdentifier OfficeOpenXmlPresentation::register_element_(
    const pugi::xml_node node, const ElementIdentifier parent,
    const ElementIdentifier previous_sibling) {
  static std::unordered_map<std::string, ElementType> element_type_table{
      {"p:sld", ElementType::SLIDE},
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

  if (element_type == ElementType::SLIDE) {
    register_children_(node.child("p:cSld").child("p:spTree"), new_element, {});
  } else {
    register_children_(node, new_element, {});
  }

  return new_element;
}

std::pair<ElementIdentifier, ElementIdentifier>
OfficeOpenXmlPresentation::register_children_(
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

ElementIdentifier OfficeOpenXmlPresentation::new_element_(
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

OfficeOpenXmlPresentation::Element *
OfficeOpenXmlPresentation::element_(const ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

const OfficeOpenXmlPresentation::Element *
OfficeOpenXmlPresentation::element_(const ElementIdentifier element_id) const {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

bool OfficeOpenXmlPresentation::editable() const noexcept { return false; }

bool OfficeOpenXmlPresentation::savable(
    const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlPresentation::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlPresentation::save(const common::Path & /*path*/,
                                     const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlPresentation::document_type() const noexcept {
  return DocumentType::PRESENTATION;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlPresentation::files() const noexcept {
  return m_filesystem;
}

ElementIdentifier OfficeOpenXmlPresentation::root_element() const {
  return m_root;
}

ElementIdentifier OfficeOpenXmlPresentation::first_entry_element() const {
  return element_first_child(m_root);
}

ElementType OfficeOpenXmlPresentation::element_type(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return ElementType::NONE;
  }
  return element_(element_id)->type;
}

ElementIdentifier OfficeOpenXmlPresentation::element_parent(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->parent;
}

ElementIdentifier OfficeOpenXmlPresentation::element_first_child(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->first_child;
}

ElementIdentifier OfficeOpenXmlPresentation::element_previous_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->previous_sibling;
}

ElementIdentifier OfficeOpenXmlPresentation::element_next_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->next_sibling;
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlPresentation::element_properties(
    ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  if (element->type == ElementType::SLIDE) {
    result[ElementProperty::WIDTH] = m_slide_width;
    result[ElementProperty::HEIGHT] = m_slide_height;
  }

  // TODO

  return result;
}

void OfficeOpenXmlPresentation::update_element_properties(
    const ElementIdentifier /*element_id*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OfficeOpenXmlPresentation::table(const ElementIdentifier /*element_id*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
