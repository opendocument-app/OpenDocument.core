#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <unordered_map>

namespace odr::internal::odf {

OpenDocument::OpenDocument(
    const DocumentType document_type,
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_document_type{document_type}, m_filesystem{std::move(filesystem)} {
  m_content_xml = util::xml::parse(*m_filesystem, "content.xml");

  if (m_filesystem->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_filesystem, "styles.xml");
  }

  m_root = register_tree_(
      m_content_xml.document_element().child("office:body").first_child(), 0,
      0);

  if (!m_root) {
    throw NoOpenDocumentFile();
  }

  // TODO scan styles

  // TODO
  // m_styles = Styles(m_styles_xml.document_element(),
  // m_content_xml.document_element());
}

ElementIdentifier
OpenDocument::register_tree_(const pugi::xml_node node,
                             const ElementIdentifier parent,
                             const ElementIdentifier previous_sibling) {
  if (!node) {
    return 0;
  }

  ElementType element_type = ElementType::NONE;

  if (node.type() == pugi::node_pcdata) {
    element_type = ElementType::TEXT;
  } else if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h") {
      element_type = ElementType::PARAGRAPH;
    } else if (element == "text:span") {
      element_type = ElementType::SPAN;
    } else if (element == "text:s" || element == "text:tab") {
      element_type = ElementType::TEXT;
    } else if (element == "text:line-break") {
      element_type = ElementType::LINE_BREAK;
    } else if (element == "text:a") {
      element_type = ElementType::LINK;
    } else if (element == "text:table-of-content") {
      return register_tree_(node.child("text:index-body").first_child(), parent,
                            previous_sibling);
    } else if (element == "text:bookmark" || element == "text:bookmark-start") {
      element_type = ElementType::BOOKMARK;
    } else if (element == "text:list") {
      element_type = ElementType::LIST;
    } else if (element == "table:table") {
      element_type = ElementType::TABLE;
    } else if (element == "draw:frame") {
      element_type = ElementType::FRAME;
    } else if (element == "draw:g") {
      // drawing group not supported
      return register_tree_(node.first_child(), parent, previous_sibling);
    } else if (element == "draw:image") {
      element_type = ElementType::IMAGE;
    } else if (element == "draw:rect") {
      element_type = ElementType::RECT;
    } else if (element == "draw:line") {
      element_type = ElementType::LINE;
    } else if (element == "draw:circle") {
      element_type = ElementType::CIRCLE;
    } else if (element == "office:text" || element == "office:presentation" ||
               element == "office:spreadsheet" || element == "office:drawing") {
      element_type = ElementType::ROOT;
    }
    // TODO if (element == "draw:custom-shape")

    // TODO log element
  }

  if (element_type == ElementType::NONE) {
    return 0;
  }

  Element element;
  element.node = node;
  element.type = element_type;
  element.parent = parent;
  element.previous_sibling = previous_sibling;
  const ElementIdentifier element_id = register_element_(element);
  if (parent && !previous_sibling) {
    element_(parent)->first_child = element_id;
  }
  if (previous_sibling) {
    element_(previous_sibling)->next_sibling = element_id;
  }

  ElementIdentifier previous_sibling_id;
  for (auto &&child_node : node) {
    const ElementIdentifier child_id =
        register_tree_(child_node, element_id, previous_sibling_id);
    previous_sibling_id = child_id;
  }

  return element_id;
}

ElementIdentifier OpenDocument::register_element_(const Element &element) {
  m_elements.push_back(element);
  return m_elements.size();
}

OpenDocument::Element *
OpenDocument::element_(const ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

const OpenDocument::Element *
OpenDocument::element_(const ElementIdentifier element_id) const {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(bool encrypted) const noexcept { return !encrypted; }

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

void OpenDocument::save(const common::Path &path, const char *password) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

DocumentType OpenDocument::document_type() const noexcept {
  return m_document_type;
}

ElementIdentifier OpenDocument::root_element() const { return m_root; }

ElementIdentifier OpenDocument::first_entry_element() const {
  return 0; // TODO
}

ElementType
OpenDocument::element_type(const ElementIdentifier element_id) const {
  if (!element_id) {
    return ElementType::NONE;
  }
  return element_(element_id)->type;
}

ElementIdentifier
OpenDocument::element_parent(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->parent;
}

ElementIdentifier
OpenDocument::element_first_child(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->first_child;
}

ElementIdentifier OpenDocument::element_previous_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->previous_sibling;
}

ElementIdentifier
OpenDocument::element_next_sibling(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->next_sibling;
}

const char *
OpenDocument::element_string_property(const ElementIdentifier element_id,
                                      const ElementProperty property) const {
  return ""; // TODO
}

std::uint32_t
OpenDocument::element_uint32_property(const ElementIdentifier element_id,
                                      const ElementProperty property) const {
  return 0; // TODO
}

bool OpenDocument::element_bool_property(const ElementIdentifier element_id,
                                         const ElementProperty property) const {
  return false; // TODO
}

const char *OpenDocument::element_optional_string_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  return ""; // TODO
}

TableDimensions
OpenDocument::table_dimensions(const ElementIdentifier element_id,
                               const std::uint32_t limit_rows,
                               const std::uint32_t limit_cols) const {
  return {}; // TODO
}

std::shared_ptr<abstract::File>
OpenDocument::image_file(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }

  if (element_(element_id)->type != ElementType::IMAGE) {
    return {};
  }

  if (!element_bool_property(element_id, ElementProperty::IMAGE_INTERNAL)) {
    // TODO support external files
    throw std::runtime_error("not internal image");
  }

  const common::Path path =
      element_string_property(element_id, ElementProperty::HREF);
  return m_filesystem->open(path);
}

void OpenDocument::set_element_string_property(
    const ElementIdentifier element_id, const ElementProperty property,
    const char *value) const {
  throw UnsupportedOperation();
}

void OpenDocument::remove_element_property(
    const ElementIdentifier element_id, const ElementProperty property) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::odf
