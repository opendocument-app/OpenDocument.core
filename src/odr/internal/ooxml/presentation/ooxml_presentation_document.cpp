#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

namespace odr::internal::ooxml::presentation {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "ppt/presentation.xml");

  m_root_element = parse_tree(*this, m_document_xml.document_element());

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

abstract::Element *Document::root_element() const { return m_root_element; }

void Document::register_element_(std::unique_ptr<Element> element) {
  m_elements.push_back(std::move(element));
}

} // namespace odr::internal::ooxml::presentation
