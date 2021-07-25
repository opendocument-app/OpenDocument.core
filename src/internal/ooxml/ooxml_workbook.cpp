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

OfficeOpenXmlWorkbook::Element *
OfficeOpenXmlWorkbook::element_(const ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

const OfficeOpenXmlWorkbook::Element *
OfficeOpenXmlWorkbook::element_(const ElementIdentifier element_id) const {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
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

ElementIdentifier OfficeOpenXmlWorkbook::root_element() const { return m_root; }

ElementIdentifier OfficeOpenXmlWorkbook::first_entry_element() const {
  return element_first_child(m_root);
}

ElementType
OfficeOpenXmlWorkbook::element_type(const ElementIdentifier element_id) const {
  if (!element_id) {
    return ElementType::NONE;
  }
  return element_(element_id)->type;
}

ElementIdentifier OfficeOpenXmlWorkbook::element_parent(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->parent;
}

ElementIdentifier OfficeOpenXmlWorkbook::element_first_child(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->first_child;
}

ElementIdentifier OfficeOpenXmlWorkbook::element_previous_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->previous_sibling;
}

ElementIdentifier OfficeOpenXmlWorkbook::element_next_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->next_sibling;
}

std::unordered_map<ElementProperty, std::any>
OfficeOpenXmlWorkbook::element_properties(ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  // TODO

  return result;
}

void OfficeOpenXmlWorkbook::update_element_properties(
    const ElementIdentifier /*element_id*/,
    std::unordered_map<ElementProperty, std::any> /*properties*/) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OfficeOpenXmlWorkbook::table(const ElementIdentifier /*element_id*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml
