#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_table.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

OpenDocument::OpenDocument(
    const DocumentType document_type,
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_document_type{document_type}, m_filesystem{std::move(filesystem)} {
  m_content_xml = util::xml::parse(*m_filesystem, "content.xml");

  if (m_filesystem->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_filesystem, "styles.xml");
  }

  m_style =
      Style(m_content_xml.document_element(), m_styles_xml.document_element());

  m_root = register_element_(
      odf::Element(
          m_content_xml.document_element().child("office:body").first_child()),
      {}, {});

  if (!m_root) {
    throw NoOpenDocumentFile();
  }
}

ElementIdentifier
OpenDocument::register_element_(const odf::Element element,
                                const ElementIdentifier parent,
                                const ElementIdentifier previous_sibling) {
  auto element_type = element.type();
  auto new_element = new_element_(element, parent, previous_sibling);

  if (element_type == ElementType::TABLE) {
    register_table_children_(element.table(), new_element);
  } else if (element_type == ElementType::SLIDE) {
    register_slide_children_(element, new_element);
  } else {
    register_children_(element, new_element, {});
  }

  return new_element;
}

std::pair<ElementIdentifier, ElementIdentifier>
OpenDocument::register_children_(const odf::Element element,
                                 const ElementIdentifier parent,
                                 ElementIdentifier previous_sibling) {
  ElementIdentifier first_element;

  for (auto &&child_element : element.children()) {
    const ElementIdentifier child =
        register_element_(child_element, parent, previous_sibling);
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

void OpenDocument::register_table_children_(const odf::TableElement element,
                                            const ElementIdentifier parent) {
  m_tables[parent] = std::make_shared<Table>(*this, element);
}

void OpenDocument::register_slide_children_(const odf::Element element,
                                            const ElementIdentifier parent) {
  auto node = element.xml_node();

  ElementIdentifier inner_previous_sibling;
  if (auto master_page_name_attr = node.attribute("draw:master-page-name")) {
    auto master_page_node =
        m_style.master_page_node(master_page_name_attr.value());
    for (auto &&child_node : master_page_node) {
      if (child_node.attribute("presentation:placeholder").as_bool()) {
        continue;
      }
      auto child = register_element_(odf::Element(child_node), parent,
                                     inner_previous_sibling);
      if (!child) {
        continue;
      }
      inner_previous_sibling = child;
    }
  }

  register_children_(element, parent, inner_previous_sibling);
}

ElementIdentifier
OpenDocument::new_element_(const odf::Element element,
                           const ElementIdentifier parent,
                           const ElementIdentifier previous_sibling) {
  auto result =
      Document::new_element_(element.type(), nullptr, parent, previous_sibling);
  m_odf_elements[result] = element;
  return result;
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void OpenDocument::save(const common::Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file("mimetype")) {
    archive.insert_file(std::end(archive), "mimetype",
                        m_filesystem->open("mimetype"), 0);
  }

  for (auto walker = m_filesystem->file_walker("/"); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (p == "mimetype") {
      continue;
    }
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == "content.xml") {
      // TODO stream
      std::stringstream out;
      m_content_xml.print(out);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream(path.path());
  archive.save(ostream);
}

void OpenDocument::save(const common::Path & /*path*/,
                        const char * /*password*/) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

DocumentType OpenDocument::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem>
OpenDocument::files() const noexcept {
  return m_filesystem;
}

std::unordered_map<ElementProperty, std::any>
OpenDocument::element_properties(const ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  auto element = m_elements[element_id];

  if (element.type == ElementType::ROOT) {
    auto style_properties = m_style.resolve_master_page(
        element.type, m_style.first_master_page().value());
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  {
    auto odf_element = m_odf_elements[element_id];
    auto element_properties = odf_element.properties();
    result.insert(std::begin(element_properties), std::end(element_properties));
  }

  if (auto style_name_it = result.find(ElementProperty::STYLE_NAME);
      style_name_it != std::end(result)) {
    auto style_name = std::any_cast<const char *>(style_name_it->second);
    auto style_properties = m_style.resolve_style(element.type, style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  // TODO this check does not need to happen all the time
  if (auto master_page_name_it = result.find(ElementProperty::MASTER_PAGE_NAME);
      master_page_name_it != std::end(result)) {
    auto master_page_name =
        std::any_cast<const char *>(master_page_name_it->second);
    auto style_properties =
        m_style.resolve_master_page(element.type, master_page_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

} // namespace odr::internal::odf
