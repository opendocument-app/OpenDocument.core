#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/util/xml_util.h>
#include <odr/document_meta.h>
#include <odr/table_dimensions.h>

namespace odr::internal::odf {

OpenDocument::OpenDocument(std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_files{std::move(files)} {
  m_content_xml = util::xml::parse(*m_files, "content.xml");

  if (m_files->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_files, "styles.xml");
  }
  // TODO
  // m_styles = Styles(m_styles_xml.document_element(),
  // m_content_xml.document_element());
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(bool encrypted) const noexcept { return !encrypted; }

void OpenDocument::save(const common::Path &path) const {
  // TODO

  /*
  // TODO throw if not savable
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipWriter writer(path);

  // `mimetype` has to be the first file and uncompressed
  if (m_files->is_file("mimetype")) {
    const auto in = m_files->read("mimetype");
    const auto out = writer.write("mimetype", 0);
    util::stream::pipe(*in, *out);
  }

  m_files->visit([&](const auto &p) {
    std::cout << p.string() << std::endl;
    if (p == "mimetype") {
      return;
    }
    if (m_files->is_directory(p)) {
      writer.createDirectory(p);
      return;
    }
    const auto in = m_files->read(p);
    const auto out = writer.write(p);
    if (p == "content.xml") {
      m_content_xml.print(*out);
      return;
    }
    util::stream::pipe(*in, *out);
  });
   */
}

void OpenDocument::save(const common::Path &path, const char *password) const {
  // TODO throw if not savable
}

DocumentType OpenDocument::document_type() const noexcept {
  return DocumentType::UNKNOWN; // TODO
}

std::uint32_t OpenDocument::entry_count() const {
  return 0; // TODO
}

abstract::ElementIdentifier OpenDocument::root_element() const {
  return 0; // TODO
}

abstract::ElementIdentifier OpenDocument::first_entry_element() const {
  return 0; // TODO
}

ElementType
OpenDocument::element_type(const abstract::ElementIdentifier element_id) const {
  return ElementType::NONE; // TODO
}

abstract::ElementIdentifier OpenDocument::element_parent(
    const abstract::ElementIdentifier element_id) const {
  return 0; // TODO
}

abstract::ElementIdentifier OpenDocument::element_first_child(
    const abstract::ElementIdentifier element_id) const {
  return 0; // TODO
}

abstract::ElementIdentifier OpenDocument::element_previous_sibling(
    const abstract::ElementIdentifier element_id) const {
  return 0; // TODO
}

abstract::ElementIdentifier OpenDocument::element_next_sibling(
    const abstract::ElementIdentifier element_id) const {
  return 0; // TODO
}

std::any
OpenDocument::element_property(const abstract::ElementIdentifier element_id,
                               const ElementProperty property) const {
  return {}; // TODO
}

const char *OpenDocument::element_string_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property) const {
  return ""; // TODO
}

std::uint32_t OpenDocument::element_uint32_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property) const {
  return 0; // TODO
}

bool OpenDocument::element_bool_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property) const {
  return false; // TODO
}

const char *OpenDocument::element_optional_string_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property) const {
  return ""; // TODO
}

TableDimensions
OpenDocument::table_dimensions(const abstract::ElementIdentifier element_id,
                               const std::uint32_t limit_rows,
                               const std::uint32_t limit_cols) const {
  return {}; // TODO
}

std::shared_ptr<abstract::File>
OpenDocument::image_file(const abstract::ElementIdentifier element_id) const {
  return {}; // TODO
}

void OpenDocument::set_element_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property, const std::any &value) const {
  // TODO
}

void OpenDocument::set_element_string_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property, const char *value) const {
  // TODO
}

void OpenDocument::remove_element_property(
    const abstract::ElementIdentifier element_id,
    const ElementProperty property) const {
  // TODO
}

} // namespace odr::internal::odf
