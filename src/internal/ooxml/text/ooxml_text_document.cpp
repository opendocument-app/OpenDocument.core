#include <internal/common/path.h>
//#include <internal/ooxml/text/ooxml_text_cursor.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::ooxml::text {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  // m_style_registry = StyleRegistry(m_styles_xml.document_element());
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

DocumentType Document::document_type() const noexcept {
  return DocumentType::TEXT;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor> Document::root_element() const {
  // return std::make_unique<DocumentCursor>(this,
  // m_document_xml.document_element().first_child());
  return {};
}

} // namespace odr::internal::ooxml::text
