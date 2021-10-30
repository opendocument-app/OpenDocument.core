#include <internal/common/path.h>
#include <internal/ooxml/ooxml_util.h>
#include <internal/ooxml/presentation/ooxml_presentation_cursor.h>
#include <internal/ooxml/presentation/ooxml_presentation_document.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml::presentation {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "ppt/presentation.xml");

  for (auto relationships :
       parse_relationships(*m_filesystem, "ppt/presentation.xml")) {
    m_slides_xml[relationships.first] = util::xml::parse(
        *m_filesystem, common::Path("ppt").join(relationships.second));
  }
}

bool Document::editable() const noexcept { return false; }

bool Document::savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const common::Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const common::Path & /*path*/,
                    const char * /*password*/) const {
  throw UnsupportedOperation();
}

FileType Document::file_type() const noexcept {
  return FileType::office_open_xml_presentation;
}

DocumentType Document::document_type() const noexcept {
  return DocumentType::presentation;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor> Document::root_element() const {
  return std::make_unique<DocumentCursor>(this,
                                          m_document_xml.document_element());
}

} // namespace odr::internal::ooxml::presentation
