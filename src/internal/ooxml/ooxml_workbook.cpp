#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/ooxml_workbook.h>
#include <internal/util/map_util.h>
#include <internal/util/xml_util.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml {

namespace {} // namespace

OfficeOpenXmlWorkbook::OfficeOpenXmlWorkbook(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  auto workbook_xml = util::xml::parse(*m_filesystem, "xl/workbook.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "xl/styles.xml");

  m_root = new_element_({}, ElementType::ROOT, {}, {});

  auto relationships = parse_relationships(*m_filesystem, "xl/workbook.xml");
  ElementIdentifier previous_sibling;
  for (auto sheet : workbook_xml.document_element().child("sheets")) {
    auto sheet_rid = sheet.attribute("r:id").value();
    auto sheet_path = common::Path("xl").join(relationships.at(sheet_rid));
    auto sheet_xml = util::xml::parse(*m_filesystem, sheet_path);

    previous_sibling = register_element_(sheet_xml.document_element(), m_root,
                                         previous_sibling);

    m_sheets_xml.push_back(std::move(sheet_xml));
  }
}

ElementIdentifier OfficeOpenXmlWorkbook::register_element_(
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
OfficeOpenXmlWorkbook::register_children_(const pugi::xml_node node,
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

ElementIdentifier OfficeOpenXmlWorkbook::new_element_(
    const pugi::xml_node node, const ElementType type,
    const ElementIdentifier parent, const ElementIdentifier previous_sibling) {
  auto result = Document::new_element_(type, parent, previous_sibling);
  m_element_nodes[result] = node;
  return result;
}

pugi::xml_node
OfficeOpenXmlWorkbook::element_node_(const ElementIdentifier element_id) const {
  return m_element_nodes.at(element_id);
}

bool OfficeOpenXmlWorkbook::editable() const noexcept { return false; }

bool OfficeOpenXmlWorkbook::savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void OfficeOpenXmlWorkbook::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void OfficeOpenXmlWorkbook::save(const common::Path & /*path*/,
                                 const char * /*password*/) const {
  throw UnsupportedOperation();
}

DocumentType OfficeOpenXmlWorkbook::document_type() const noexcept {
  return DocumentType::PRESENTATION;
}

std::shared_ptr<abstract::ReadableFilesystem>
OfficeOpenXmlWorkbook::files() const noexcept {
  return m_filesystem;
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlWorkbook::element_properties(ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  auto element = m_elements[element_id];

  // TODO

  return result;
}

void OfficeOpenXmlWorkbook::update_element_properties(
    const ElementIdentifier /*element_id*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
