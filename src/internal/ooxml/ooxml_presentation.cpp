#include <internal/common/path.h>
#include <internal/ooxml/ooxml_presentation.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/util/map_util.h>
#include <internal/util/property_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

namespace {
void read_xfrm_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  const auto off = node.child("a:off");
  const auto ext = node.child("a:ext");

  util::property::set_optional_property(
      ElementProperty::X, read_emus_attribute(off.attribute("x")), result);
  util::property::set_optional_property(
      ElementProperty::Y, read_emus_attribute(off.attribute("y")), result);
  util::property::set_optional_property(
      ElementProperty::WIDTH, read_emus_attribute(ext.attribute("cx")), result);
  util::property::set_optional_property(
      ElementProperty::HEIGHT, read_emus_attribute(ext.attribute("cy")),
      result);
}

void resolve_text_element_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  util::property::set_optional_property(ElementProperty::TEXT,
                                        read_text_property(node), result);
}

void resolve_frame_element_properties(
    const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  read_xfrm_properties(node.child("a:xfrm"), result);
}

void resolve_element_properties(
    const ElementType element, const pugi::xml_node node,
    std::unordered_map<ElementProperty, std::any> &result) {
  if (element == ElementType::TEXT) {
    resolve_text_element_properties(node, result);
  } else if (element == ElementType::FRAME) {
    resolve_frame_element_properties(node.child("p:spPr"), result);
  }
}
} // namespace

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
  for (auto slide : presentation_xml.document_element().child("p:sldIdLst")) {
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
      {"p:sp", ElementType::FRAME},
      {"p:graphicFrame", ElementType::FRAME},
      {"a:p", ElementType::PARAGRAPH},
      {"a:r", ElementType::SPAN},
      {"a:t", ElementType::TEXT},
      {"w:tab", ElementType::TEXT},
  };

  if (!node) {
    return {};
  }

  ElementType element_type = ElementType::NONE;

  const std::string element = node.name();
  if (node.type() == pugi::node_element) {
    util::map::lookup_map(element_type_table, element, element_type);
  }

  if (element_type == ElementType::NONE) {
    // TODO log node
    return {};
  }

  auto new_element = new_element_(node, element_type, parent, previous_sibling);

  if (element_type == ElementType::SLIDE) {
    register_children_(node.child("p:cSld").child("p:spTree"), new_element, {});
  } else if (element == "p:sp") {
    register_children_(node.child("p:txBody"), new_element, {});
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
  auto result = Document::new_element_(type, parent, previous_sibling);
  m_element_nodes[result] = node;
  return result;
}

pugi::xml_node OfficeOpenXmlPresentation::element_node_(
    const ElementIdentifier element_id) const {
  return m_element_nodes.at(element_id);
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

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlPresentation::element_properties(
    ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  // TODO move?
  if (element->type == ElementType::SLIDE) {
    result[ElementProperty::WIDTH] = m_slide_width;
    result[ElementProperty::HEIGHT] = m_slide_height;
  }

  resolve_element_properties(element->type, element_node_(element_id), result);

  return result;
}

void OfficeOpenXmlPresentation::update_element_properties(
    const ElementIdentifier /*element_id*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
